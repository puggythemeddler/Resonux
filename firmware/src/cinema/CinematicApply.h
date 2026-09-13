#pragma once
#include "cinema/CinematicEngine.h"
#include "theme/Theme.h"
#include <stdint.h>

// Bridge between the pure cinematic layer and the theme engine output (spec
// §36). Everything enters the pipeline as a modulation of the ThemeFrame the
// ThemeEngine already produced; actual pin driving stays in ThemeEngine ->
// Effects -> LEDDriver, so Cinematic Mode can never bypass brightness/power
// management.

namespace cine {

// Clamp a float into [0,1].
inline float cap01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

// Applies a cinematic Look on top of a processed theme frame. Deliberately
// conservative: modulation multiplies/offsets, never replaces, the underlying
// theme — a theme always stays recognizable.
//
// `zoneScale` is the spatial wave-field response of this strip (0..1, from
// SpatialWaveField::intensityAt) mapped by the caller into a brightness
// multiplier. Leave the default 1.0 (or pass it explicitly) for exactly the
// pre-mapping behaviour — room mapping OFF means the formula is unchanged.
inline void applyToThemeFrame(Themes::ThemeFrame& th, const Look& lk,
                              const Config& cfg, float zoneScale = 1.0f) {
  if (zoneScale < 0.0f) zoneScale = 0.0f;
  if (zoneScale > 1.0f) zoneScale = 1.0f;
  th.brightness = cap01(th.brightness * lk.brightness * zoneScale);
  th.saturation = cap01(th.saturation * lk.saturation);
  th.colourShift = cap01(th.colourShift + lk.hueShift);
  th.movement = cap01(th.movement + lk.movement);
  th.pulse = cap01(th.pulse + lk.pulse);
  th.beatFlash = cap01(th.beatFlash + lk.flash * 0.55f);
  th.sparkle = cap01(th.sparkle + lk.flash * 0.25f);
  th.intensity = cap01(th.intensity * (1.0f - lk.calm * 0.5f));
  th.transitionSpeed =
      cap01(th.transitionSpeed * (1.0f - cfg.smoothing * 0.6f));

  if (lk.tintMix > 0.001f && th.paletteCount > 0) {
    const float m = lk.tintMix;
    const Rgb base = th.palette ? th.palette[0] : th.primary;
    th.primary = blend(base, lk.tint, m);
    th.accent = blend(th.accent, lk.tint, m * 0.7f);
    th.secondary = blend(th.secondary, lk.tint, m * 0.45f);
    th.background = blend(th.background, lk.tint, m * 0.25f);
  }
}

}  // namespace cine