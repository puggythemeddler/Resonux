#pragma once
#include <stdint.h>

struct EffectParams {
  uint8_t   maxBrightness = 255;
  uint8_t   minBrightness = 12;
  float     sensitivity = 1.0f;
  uint8_t   startHue = 0;
  float     hueSpeed = 8.0f;
  float     decay = 0.0f;
  int       palette = 0;
  uint8_t   saturation = 255;
  int       zoneCount = 0;
  const int* zoneBand = nullptr;
  int       bandCount = 0;
};