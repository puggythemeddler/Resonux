#pragma once
#include "audio/AudioFrame.h"
#include "util/Rgb.h"
#include <stdint.h>

namespace Themes {

constexpr int kMaxThemes       = 24;
constexpr int kMaxThemeColors  = 8;
constexpr int kMaxIdLen        = 24;
constexpr int kMaxNameLen      = 24;

// Neutral built-in theme used whenever the selected id is missing/empty.
constexpr const char kDefaultThemeId[]  = "classic";
constexpr const char kAutoThemeId[]     = "auto";

// A single user-visible lighting personality. Pure data — no hardware.
struct ThemeDef {
  char     id[kMaxIdLen]   = "";
  char     name[kMaxNameLen] = "";
  Rgb      palette[kMaxThemeColors];
  uint8_t  paletteCount    = 0;
  uint8_t  brightnessPct   = 100;  // 0..100 base brightness target
  uint8_t  saturation      = 255;  // 0..255
  // per-frequency response scales (audio -> output)
  float    bassResp        = 1.0f;
  float    midResp         = 1.0f;
  float    trebleResp      = 1.0f;
  float    beatResp        = 1.0f;
  float    ampResp         = 1.0f;
  // animation character
  float    movement        = 0.5f;  // 0..1 has-beat-independent motion speed
  float    pulse           = 0.5f;  // 0..1 beat pulse strength
  float    sparkle         = 0.3f;  // 0..1 treble sparkle strength
  float    smoothing       = 0.3f;  // 0..1 decay/smoothing (higher=slower)
  bool     builtin         = false; // builtins cannot be deleted
};

// Output of the theme engine, consumed by the effects system.
struct ThemeFrame {
  float         intensity      = 1.0f;  // overall energy 0..1
  float         bassResponse   = 0.0f;  // 0..1  (already response-scaled)
  float         midResponse    = 0.0f;
  float         trebleResponse = 0.0f;
  float         movement       = 0.0f;  // 0..1 motion speed factor
  float         pulse          = 0.0f;  // 0..1 beat pulse strength
  float         sparkle        = 0.0f;  // 0..1 treble sparkle strength
  float         colourShift    = 0.0f;  // 0..1 hue offset (fraction)
  float         saturation     = 1.0f;  // 0..1
  float         brightness     = 1.0f;  // 0..1 multiplier on strip brightness
  float         beatResponse   = 0.0f;  // 0..1
  float         smoothing      = 0.0f;  // 0..1 (decay amount)
  const Rgb*    palette        = nullptr;  // null => use strip enum palette
  int           paletteCount   = 0;
  const char*   themeId        = nullptr;
  const char*   themeName      = nullptr;
};

}  // namespace Themes