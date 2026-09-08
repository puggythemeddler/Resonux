#pragma once
#include "util/Rgb.h"

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

  // Per-frame fields injected by StripRuntime from the active ThemeFrame.
  // Effects only read these via the EffUtil helpers below.
  float       themeBright = 1.0f;        // 0..1 output brightness multiplier
  float       themeMovement = 1.0f;      // 0..1 motion speed factor
  float       themePulse = 0.0f;         // 0..1 beat pulse depth
  float       themeFlash = 0.0f;         // 0..1 beat flash aggressiveness
  float       themeSparkle = 0.0f;       // 0..1 treble sparkle strength
  float       themeEnergy = 0.5f;        // 0..1 overall audio energy
  float       themeHueOffset = 0.0f;     // 0..1 hue shift (fraction)
  float       themeSaturation = 1.0f;    // 0..1 saturation multiplier
  float       themeBeat = 0.0f;          // 0..1 beat presence
  float       themeDensity = 0.6f;       // 0..1 visual density
  const Rgb*  themePalette = nullptr;    // override gradient when non-null
  int         themePaletteCount = 0;
};