#pragma once
#include "audio/AudioFrame.h"
#include "util/Rgb.h"
#include <stdint.h>

namespace Themes {

constexpr int kMaxThemes       = 24;
constexpr int kMaxThemeColors  = 8;
constexpr int kMaxIdLen        = 24;
constexpr int kMaxNameLen      = 24;
constexpr int kMaxDescLen      = 48;
constexpr int kMaxPrefEffect   = 5;

// Neutral built-in theme used whenever the selected id is missing/empty.
constexpr const char kDefaultThemeId[]  = "classic";
constexpr const char kAutoThemeId[]     = "auto";

// A single user-visible lighting personality. Pure data — no hardware.
struct ThemeDef {
  char     id[kMaxIdLen]   = "";
  char     name[kMaxNameLen] = "";
  char     description[kMaxDescLen] = "";
  Rgb      palette[kMaxThemeColors];
  uint8_t  paletteCount    = 0;
  uint8_t  brightnessPct   = 100;  // 0..100 base brightness target
  uint8_t  minBrightnessPct = 15;  // 0..100 dimmest floor (<= base)
  uint8_t  saturation      = 255;  // 0..255
  // per-frequency response scales (audio -> output)
  float    bassResp        = 1.0f;
  float    lowMidResp      = 1.0f;
  float    midResp         = 1.0f;
  float    highMidResp     = 1.0f;
  float    trebleResp      = 1.0f;
  float    beatResp        = 1.0f;
  float    ampResp         = 1.0f;
  // animation character
  float    movement        = 0.5f;  // 0..1 has-beat-independent motion speed
  float    pulse           = 0.5f;  // 0..1 beat pulse strength (depth, not flash)
  float    beatFlash       = 0.3f;  // 0..1 full-frame beat flash aggressiveness
  float    sparkle         = 0.3f;  // 0..1 treble sparkle strength
  float    smoothing       = 0.3f;  // 0..1 decay/smoothing (higher=slower)
  float    contrast        = 0.5f;  // 0..1 brightness range stretching
  float    density         = 0.5f;  // 0..1 visual density / amount of fixtures lit
  // preferred effect hints (informational; user override always wins)
  int      preferredEffects[kMaxPrefEffect];
  uint8_t  prefEffectCount = 0;
  bool     builtin         = false; // builtins cannot be deleted
};

// Output of the theme engine, consumed by the effects system.
struct ThemeFrame {
  float         intensity      = 1.0f;  // overall energy 0..1
  float         energy         = 1.0f;  // alias of intensity (spec name)
  float         bassResponse   = 0.0f;  // 0..1  (already response-scaled)
  float         lowMidResponse = 0.0f;  // 0..1
  float         midResponse    = 0.0f;
  float         highMidResponse = 0.0f;
  float         trebleResponse = 0.0f;
  float         movement       = 0.0f;  // 0..1 motion speed factor
  float         pulse          = 0.0f;  // 0..1 beat pulse depth
  float         beatFlash      = 0.0f;  // 0..1 beat flash aggressiveness
  float         sparkle        = 0.0f;  // 0..1 treble sparkle strength
  float         colourShift    = 0.0f;  // 0..1 hue offset (fraction)
  float         saturation     = 1.0f;  // 0..1
  float         brightness     = 1.0f;  // 0..1 multiplier on strip brightness
  float         beatResponse   = 0.0f;  // 0..1
  float         smoothing      = 0.0f;  // 0..1 (decay amount)
  float         contrast       = 0.5f;  // 0..1
  float         density        = 0.5f;  // 0..1
  float         transitionSpeed = 0.5f; // 0..1 (inverse of smoothing)
  const Rgb*    palette        = nullptr;  // null => use strip enum palette
  int           paletteCount   = 0;
  Rgb           primary;       // palette[0]
  Rgb           secondary;     // palette[1]
  Rgb           accent;        // palette[2]
  Rgb           background;    // palette[last]
  int           preferredEffect = -1;  // EffectId hint, -1 = none
  const char*   themeId        = nullptr;
  const char*   themeName      = nullptr;
};

}  // namespace Themes