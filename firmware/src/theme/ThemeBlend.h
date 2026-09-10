#pragma once
#include "theme/Theme.h"
#include "util/Rgb.h"

// Pure theme-frame interpolation used for theme transitions. Kept outside the
// ThemeEngine so the math is host-unit-testable without FreeRTOS/Arduino.
namespace Themes {

inline float lerpT(float a, float b, float t) { return a + (b - a) * t; }
inline uint8_t lerpU8(float a, float b, float t) {
  return (uint8_t)(a + (b - a) * t + (a <= b ? 0.5f : -0.5f));
}
inline Rgb lerpRgb(const Rgb& a, const Rgb& b, float t) {
  Rgb out;
  out.r = lerpU8(a.r, b.r, t);
  out.g = lerpU8(a.g, b.g, t);
  out.b = lerpU8(a.b, b.b, t);
  return out;
}
inline float easeT(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return t * t * (3.0f - 2.0f * t);  // smoothstep
}

// Blend `from` -> `to` at progress t in [0,1]. t=0 returns `from`, t=1 returns
// `to` verbatim. Scalar fields interpolate; independent palette colours cross-
// fade; the target palette pointer is adopted (the effects layer reads the
// Rgb fields, palette only informs strip mapping).
inline ThemeFrame blendThemeFrame(const ThemeFrame& from, const ThemeFrame& to,
                                  float t) {
  const float e = easeT(t);
  if (e <= 0.0f) return from;
  if (e >= 1.0f) return to;

  ThemeFrame o;
  o.intensity = lerpT(from.intensity, to.intensity, e);
  o.energy = o.intensity;
  o.bassResponse = lerpT(from.bassResponse, to.bassResponse, e);
  o.lowMidResponse = lerpT(from.lowMidResponse, to.lowMidResponse, e);
  o.midResponse = lerpT(from.midResponse, to.midResponse, e);
  o.highMidResponse = lerpT(from.highMidResponse, to.highMidResponse, e);
  o.trebleResponse = lerpT(from.trebleResponse, to.trebleResponse, e);
  o.movement = lerpT(from.movement, to.movement, e);
  o.pulse = lerpT(from.pulse, to.pulse, e);
  o.beatFlash = lerpT(from.beatFlash, to.beatFlash, e);
  o.sparkle = lerpT(from.sparkle, to.sparkle, e);
  o.colourShift = lerpT(from.colourShift, to.colourShift, e);
  o.saturation = lerpT(from.saturation, to.saturation, e);
  o.brightness = lerpT(from.brightness, to.brightness, e);
  o.beatResponse = lerpT(from.beatResponse, to.beatResponse, e);
  o.smoothing = lerpT(from.smoothing, to.smoothing, e);
  o.contrast = lerpT(from.contrast, to.contrast, e);
  o.density = lerpT(from.density, to.density, e);
  o.transitionSpeed = lerpT(from.transitionSpeed, to.transitionSpeed, e);

  o.primary = lerpRgb(from.primary, to.primary, e);
  o.secondary = lerpRgb(from.secondary, to.secondary, e);
  o.accent = lerpRgb(from.accent, to.accent, e);
  o.background = lerpRgb(from.background, to.background, e);

  o.palette = to.palette;
  o.paletteCount = to.paletteCount > 0 ? to.paletteCount : from.paletteCount;
  o.preferredEffect = (e >= 0.5f) ? to.preferredEffect : from.preferredEffect;
  o.themeId = to.themeId;
  o.themeName = to.themeName;
  return o;
}

}  // namespace Themes