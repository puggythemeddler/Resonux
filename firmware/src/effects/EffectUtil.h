#pragma once
#include "effects/EffectParams.h"
#include "util/Rgb.h"
#include <math.h>

// Theme-injected helpers. `p.*theme*` fields are refreshed every frame by
// StripRuntime from the active ThemeFrame; the old stateless behaviour is
// recovered when they hit their neutral defaults.

// Audio gate: per-theme energy gently stretches the strip sensitivity.
inline float effLevel(const EffectParams& p, float v) {
  float x = v * p.sensitivity * (0.7f + 0.6f * p.themeEnergy);
  return clamp01(x);
}

inline Rgb effRgb(const EffectParams& p, float h01, float s, float l,
                  float level) {
  // theme hue offset + saturation apply to the colour itself
  h01 += p.themeHueOffset;
  s *= p.themeSaturation;
  if (s > 1.0f) s = 1.0f;
  Rgb c = hslToRgb(h01, s, l);
  // brightness range scaled by theme brightness; beat pulse swells the level
  float b = (float)p.minBrightness +
            (float)(p.maxBrightness - p.minBrightness) * clamp01(level);
  b *= p.themeBright;
  b *= 1.0f + p.themePulse * p.themeBeat * 0.6f;  // deep pulse, not a flash
  return scale(c, b / 255.0f);
}

inline float effHueOffset(const EffectParams& p, float tSec) {
  return (p.startHue / 360.0f) + p.themeHueOffset +
         fmodf(p.hueSpeed * (0.1f + 0.9f * p.themeMovement) * tSec, 360.0f) /
             360.0f;
}

// Theme palette gradient when present, otherwise the strip's enum palette.
inline Rgb themeColor(const EffectParams& p, float t) {
  if (p.themePalette && p.themePaletteCount > 0) {
    if (p.themePaletteCount == 1) return p.themePalette[0];
    float x = clamp01(t) * (float)(p.themePaletteCount - 1);
    int i0 = (int)x;
    int i1 = (i0 + 1 < p.themePaletteCount) ? i0 + 1 : p.themePaletteCount - 1;
    return blend(p.themePalette[i0], p.themePalette[i1], x - (float)i0);
  }
  return paletteColor(p.palette, t);
}