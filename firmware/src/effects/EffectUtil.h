#pragma once
#include "effects/EffectParams.h"
#include "util/Rgb.h"
#include <math.h>

inline float effLevel(const EffectParams& p, float v) {
  float x = v * p.sensitivity;
  return clamp01(x);
}

inline Rgb effRgb(const EffectParams& p, float h01, float s, float l,
                  float level) {
  Rgb c = hslToRgb(h01, s, l);
  float b = (float)p.minBrightness +
            (float)(p.maxBrightness - p.minBrightness) * clamp01(level);
  return scale(c, b / 255.0f);
}

inline float effHueOffset(const EffectParams& p, float tSec) {
  return (p.startHue / 360.0f) + fmodf(p.hueSpeed * tSec, 360.0f) / 360.0f;
}