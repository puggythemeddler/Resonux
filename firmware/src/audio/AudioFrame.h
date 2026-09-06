#pragma once
#include "config/ConfigDefs.h"

struct AudioFrame {
  float    bands[kMaxBands] = {0.0f};
  float    peaks[kMaxBands] = {0.0f};
  uint8_t  bandCount = 0;
  float    amplitude = 0.0f;
  float    bass = 0.0f;
  float    lowMid = 0.0f;
  float    mid = 0.0f;
  float    highMid = 0.0f;
  float    treble = 0.0f;
  bool     beat = false;
  float    beatStrength = 0.0f;
  uint32_t timeMs = 0;
};