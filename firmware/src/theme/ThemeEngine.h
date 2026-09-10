#pragma once
#include "config/Config.h"
#include "theme/Theme.h"
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

using Themes::ThemeDef;
using Themes::ThemeFrame;
using Themes::kMaxThemes;

// Data-driven, hardware-independent theme engine.
//
// Lives ABOVE the effects system: it consumes an AudioFrame and produces a
// ThemeFrame that StripRuntime maps onto EffectParams. It never touches GPIO,
// LED drivers or FastLED. Theme definitions are JSON data persisted on
// LittleFS (/themes.json); built-ins are re-installed on factory reset.
class ThemeEngine {
public:
  static ThemeEngine& instance();

  void begin(Config& cfg);

  // --- theme list ---------------------------------------------------------
  int count() const { return _count; }
  const ThemeDef* get(int index) const;
  const ThemeDef* find(const char* id) const;

  // --- selection (active global + per-strip overrides, "" => global) ------
  const char* activeId() const;                 // resolves to actual id
  const char* activeRawId() const { return _active[kMaxStrips]; }
  const char* activeStripId(int strip) const;
  bool apply(const char* id);                   // global
  bool applyStrip(int strip, const char* id);   // per-strip override

  // --- CRUD ---------------------------------------------------------------
  // upsert (id == "" -> auto-assign "customN"), validate+apply.
  bool update(const ThemeDef& t, bool upsert, char* err, size_t errLen);
  bool remove(const char* id, char* err, size_t errLen);
  void resetDefaults();
  bool saveAll();

  // --- evaluation ----------------------------------------------------------
  ThemeFrame process(const AudioFrame& a, uint32_t nowMs);  // global theme
  ThemeFrame processStrip(int strip, const AudioFrame& a, uint32_t nowMs);
  ThemeFrame processTheme(const ThemeDef& def, const AudioFrame& a,
                          uint32_t nowMs);

  // A neutral identity frame used by tests / legacy paths.
  static ThemeFrame identityFrame();

  // --- JSON helpers (used by the web API + persistence) --------------------
  static void encode(const ThemeDef& t, JsonObject obj);
  static bool decode(const JsonVariantConst& src, ThemeDef& out,
                     char* err, size_t errLen);
  static bool parseHex(const char* hex, Rgb& out);
  static const char* hexColor(const Rgb& c, char* buf);

private:
  ThemeEngine() {}
  ThemeEngine(const ThemeEngine&) = delete;

  void installDefaults();
  bool loadAll();
  const ThemeDef* resolve(const char* id) const;
  void setActiveRaw(int slot, const char* id);  // copies into cache
  bool saveAllLocked();
  ThemeFrame processAuto(const AudioFrame& a, uint32_t nowMs);
  // Smooths theme/stage switches over _transDurMs using ThemeBlend.
  ThemeFrame transition(int slot, ThemeFrame target, uint32_t nowMs);

  Config*    _cfgPtr = nullptr;
  ThemeDef   _themes[kMaxThemes];
  int        _count = 0;
  // Stable per-slot working copy so process()/processStrip() never return a
  // ThemeFrame whose .palette dangles into a stack-local ThemeDef.
  ThemeDef   _work[kMaxStrips + 1];  // [kMaxStrips] = global slot
  char       _active[kMaxStrips + 1][Themes::kMaxIdLen];  // [kMaxStrips]=global
  SemaphoreHandle_t _mutex = nullptr;

  // AUTO-mode state (hysteresis-aware classifier)
  float      _autoEnergy = 0.0f;
  float      _autoGroove = 0.0f;
  int        _autoStage = 0;
  uint32_t   _autoStageSince = 0;
  uint32_t   _autoLastMs = 0;
  uint32_t   _autoAfroMs = 0;
  uint32_t   _autoRockMs = 0;

  // Theme-transition state (cross-fade when the active theme/stage changes)
  ThemeFrame _prevFrame[kMaxStrips + 1];       // last emitted frame
  uint32_t   _transStart[kMaxStrips + 1];      // transition begin timestamp
  char       _trackTheme[kMaxStrips + 1][Themes::kMaxIdLen];  // transition key
  bool       _trackInited[kMaxStrips + 1] = {};
  uint32_t   _transDurMs = 0;                  // configured transition length
};

inline Themes::ThemeFrame ThemeEngine::identityFrame() {
  Themes::ThemeFrame f;
  f.intensity = 1.0f;
  f.energy = 1.0f;
  f.brightness = 1.0f;
  f.saturation = 1.0f;
  f.movement = 1.0f;
  f.contrast = 0.5f;
  f.density = 0.5f;
  f.transitionSpeed = 0.5f;
  f.themeId = Themes::kDefaultThemeId;
  f.themeName = "Classic";
  return f;
}