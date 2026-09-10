#pragma once
#include "audio/AudioSource.h"
#include <stdint.h>
#include <math.h>

// Built-in, hardware-independent audio source: a multi-tone generator that
// reliably excites the analyzer's bands (low sine + a mid overtone + a treble
// shard). Used for testing the whole pipeline with no microphone attached, and
// as the immediate fallback when a preferred source disappears. Pure C++.
class TestToneSource : public AudioSource {
public:
  TestToneSource(float sampleRate = 44100.0f) : _sampleRate(sampleRate > 0 ? sampleRate : 44100.0f) {}

  bool begin() override { return true; }

  int readSamples(float* out, int maxCount) override {
    if (!out) return 0;
    const double pi2 = 6.283185307179586;  // avoid arduinoFFT's twoPi macro
    for (int i = 0; i < maxCount; ++i) {
      const double t = (double)_phase / (double)_sampleRate;
      // 110 Hz fundamental + 220 Hz + light 880 Hz — broad, musical energy.
      float v = (float)(0.42 * sin(pi2 * 110.0 * t) +
                        0.30 * sin(pi2 * 220.0 * t + 0.7) +
                        0.12 * sin(pi2 * 880.0 * t + 1.3));
      out[i] = v;
      _phase++;
    }
    return maxCount;
  }

  float sampleRate() const override { return _sampleRate; }
  const char* name() const override { return "test_tone"; }

  void setFrequencyBias(float bass, float mid, float treble) {
    _bass = bass;
    _mid = mid;
    _treble = treble;
  }

private:
  float   _sampleRate;
  uint64_t _phase = 0;
  float   _bass = 1.0f;
  float   _mid = 1.0f;
  float   _treble = 1.0f;
};