#include "audio/AudioAnalyzer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static constexpr float kPeakDecay = 0.985f;
static constexpr float kAutoDecay = 0.9995f;

AudioAnalyzer::AudioAnalyzer(const AudioAnalyzerConfig& cfg, AudioSource* source)
    : _cfg(cfg), _source(source) {}

AudioAnalyzer::~AudioAnalyzer() {
  delete _fft;
  free(_window);
  free(_imag);
}

bool AudioAnalyzer::begin() {
  if (_cfg.fftSize < 64 || _cfg.sampleRate <= 0 || !_source) return false;
  _window = (float*)malloc(_cfg.fftSize * sizeof(float));
  _imag = (float*)calloc(_cfg.fftSize, sizeof(float));
  if (!_window || !_imag) return false;
  memset(_window, 0, _cfg.fftSize * sizeof(float));

  int half = _cfg.fftSize / 2;
  for (int b = 0; b < _cfg.bandCount && b < kMaxBands; ++b) {
    int lo = (int)ceilf(_cfg.bands[b].loHz * _cfg.fftSize / _cfg.sampleRate);
    int hi = (int)ceilf(_cfg.bands[b].hiHz * _cfg.fftSize / _cfg.sampleRate);
    if (lo < 1) lo = 1;
    if (hi > half) hi = half;
    if (hi <= lo) hi = lo + 1;
    _binStart[b] = lo;
    _binEnd[b] = hi;
  }

  _fft = new ArduinoFFT<float>(_window, _imag, _cfg.fftSize, _cfg.sampleRate);
  if (!_fft) return false;

  _cursor = 0;
  _frames = 0;
  _startMs = 0;
  _fps = 0.0f;
  _beat.reset();
  memset(_smooth, 0, sizeof(_smooth));
  memset(_peak, 0, sizeof(_peak));
  memset(_autoCeil, 0, sizeof(_autoCeil));
  memset(&_frame, 0, sizeof(_frame));
  return true;
}

void AudioAnalyzer::process(uint32_t nowMs) {
  int need = _cfg.fftSize - _cursor;
  if (need > 0) {
    int got = _source->readSamples(_window + _cursor, need);
    _cursor += got;
  }
  if (_cursor >= _cfg.fftSize) {
    if (_startMs == 0) _startMs = nowMs;
    runAnalysis(nowMs);
    int keep = _cfg.fftSize - _cfg.hopSize;
    if (keep < 0) keep = 0;
    memmove(_window, _window + _cfg.hopSize, keep * sizeof(float));
    _cursor = keep;
  }
}

void AudioAnalyzer::runAnalysis(uint32_t nowMs) {
  float sumSq = 0.0f;
  for (int i = 0; i < _cfg.fftSize; ++i) sumSq += _window[i] * _window[i];
  float rms = sqrtf(sumSq / _cfg.fftSize);

  memset(_imag, 0, _cfg.fftSize * sizeof(float));
  _fft->windowing(FFTWindow::Hamming, FFTDirection::Forward);
  _fft->compute(FFTDirection::Forward);
  _fft->complexToMagnitude();

  float raw[kMaxBands] = {0.0f};
  int low = _cfg.beatLowBands;
  if (low > _cfg.bandCount) low = _cfg.bandCount;
  float beatEnergy = 0.0f;
  for (int b = 0; b < _cfg.bandCount; ++b) {
    for (int i = _binStart[b]; i < _binEnd[b]; ++i) raw[b] += _window[i];
    if (b < low) beatEnergy += raw[b] * raw[b];
  }

  for (int b = 0; b < _cfg.bandCount; ++b) {
    float r = raw[b] * _cfg.gain;
    float n;
    if (_cfg.normMode == 0) {
      float db = 20.0f * log10f(r + 1e-6f);
      n = (db - _cfg.dbFloor) / (_cfg.dbCeil - _cfg.dbFloor);
      if (n < 0.0f) n = 0.0f;
      if (n > 1.0f) n = 1.0f;
    } else {
      if (r > _autoCeil[b]) _autoCeil[b] = r;
      else _autoCeil[b] *= kAutoDecay;
      if (_autoCeil[b] < 1e-6f) _autoCeil[b] = 1e-6f;
      n = r / _autoCeil[b];
      if (n > 1.0f) n = 1.0f;
    }
    if (n < _cfg.noiseGate) n = 0.0f;

    float v;
    if (_cfg.smooth) {
      float k = (n > _smooth[b]) ? _cfg.attack : _cfg.release;
      v = _smooth[b] + (n - _smooth[b]) * k;
    } else {
      v = n;
    }
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    _smooth[b] = v;
    _frame.bands[b] = v;
  }

  _frame.bandCount = _cfg.bandCount;

  if (_cfg.beatDetect) {
    detectBeat(nowMs, beatEnergy);
  } else {
    _frame.beat = false;
    _frame.beatStrength = 0.0f;
  }

  for (int b = 0; b < _cfg.bandCount; ++b) {
    if (_frame.bands[b] >= _peak[b]) _peak[b] = _frame.bands[b];
    else _peak[b] *= kPeakDecay;
    _frame.peaks[b] = _peak[b];
  }

  auto groupAvg = [&](int g) {
    int s = _cfg.groupRanges[g][0];
    int e = _cfg.groupRanges[g][1];
    if (e < s) e = s;
    if (s < 0) s = 0;
    if (e >= _cfg.bandCount) e = _cfg.bandCount - 1;
    float acc = 0.0f;
    for (int b = s; b <= e; ++b) acc += _frame.bands[b];
    return acc / (float)(e - s + 1);
  };
  _frame.bass = groupAvg(0);
  _frame.lowMid = groupAvg(1);
  _frame.mid = groupAvg(2);
  _frame.highMid = groupAvg(3);
  _frame.treble = groupAvg(4);

  float adb = 20.0f * log10f(rms * _cfg.ampGain + 1e-6f);
  float an = (adb - _cfg.ampDbFloor) / (_cfg.ampDbCeil - _cfg.ampDbFloor);
  if (an < 0.0f) an = 0.0f;
  if (an > 1.0f) an = 1.0f;
  if (an < _cfg.noiseGate) an = 0.0f;
  _frame.amplitude = an;

  _frame.timeMs = nowMs;
  ++_frames;
  uint32_t elapsed = nowMs - _startMs;
  if (elapsed > 0) _fps = 1000.0f * _frames / (float)elapsed;
}

void AudioAnalyzer::detectBeat(uint32_t nowMs, float energy) {
  bool beat = false;
  float strength = 0.0f;
  _beat.feed(energy, _cfg.beatSens, _cfg.beatMinGapMs, nowMs, beat, strength);
  _frame.beat = beat;
  _frame.beatStrength = strength;
}