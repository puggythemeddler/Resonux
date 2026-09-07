#pragma once
#include "audio/AudioSource.h"
#include "audio/AudioFrame.h"
#include "audio/BeatDetector.h"
#include "config/ConfigDefs.h"
#include <arduinoFFT.h>

struct BandDef {
  float loHz;
  float hiHz;
};

struct AudioAnalyzerConfig {
  int       sampleRate = 44100;
  int       fftSize = 1024;
  int       hopSize = 512;
  int       bandCount = 9;
  BandDef   bands[kMaxBands] = {};
  int       groupRanges[5][2] = {};
  float     gain = 1.0f;
  int       normMode = 1;
  float     dbFloor = -45.0f;
  float     dbCeil = -10.0f;
  float     noiseGate = 0.02f;
  bool      smooth = true;
  float     attack = 0.60f;
  float     release = 0.14f;
  bool      beatDetect = true;
  float     beatSens = 1.0f;
  int       beatMinGapMs = 250;
  int       beatLowBands = 3;
  float     ampGain = 1.5f;
  float     ampDbFloor = -55.0f;
  float     ampDbCeil = -12.0f;
};

class AudioAnalyzer {
public:
  AudioAnalyzer(const AudioAnalyzerConfig& cfg, AudioSource* source);
  ~AudioAnalyzer();

  bool begin();
  void process(uint32_t nowMs);

  uint32_t frameCount() const { return _frames; }
  void getFrame(AudioFrame& out) const { out = _frame; }
  const AudioFrame& frame() const { return _frame; }
  float fps() const { return _fps; }

private:
  void runAnalysis(uint32_t nowMs);
  void detectBeat(uint32_t nowMs, float energy);

  const AudioAnalyzerConfig& _cfg;
  AudioSource* _source;

  float* _window = nullptr;
  float* _imag = nullptr;
  ArduinoFFT<float>* _fft = nullptr;

  int     _binStart[kMaxBands] = {0};
  int     _binEnd[kMaxBands] = {0};
  int     _cursor = 0;

  float   _smooth[kMaxBands] = {0.0f};
  float   _peak[kMaxBands] = {0.0f};
  float   _autoCeil[kMaxBands] = {0.0f};

  AudioFrame _frame;

  uint32_t _frames = 0;
  uint32_t _startMs = 0;
  float    _fps = 0.0f;

  BeatDetector _beat;
};