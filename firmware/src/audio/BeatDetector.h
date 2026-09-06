#pragma once
#include <stdint.h>

class BeatDetector {
public:
  void reset();

  void feed(float energy, float sensitivity, int minGapMs,
            uint32_t nowMs, bool& beatOut, float& strengthOut);

private:
  float    _energyAvg = 0.0f;
  float    _energyVar = 0.0f;
  uint32_t _lastBeatAt = 0;
  bool     _warm = false;
};