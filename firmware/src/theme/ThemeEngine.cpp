#include "theme/ThemeEngine.h"
#include "config/ConfigStore.h"
#include "util/Rgb.h"
#include <LittleFS.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

using Themes::ThemeDef;
using Themes::ThemeFrame;

static const char kThemesPath[] = "/themes.json";

enum AutoStage {
  AUTO_AMBIENT = 0,
  AUTO_CHILL,
  AUTO_SPECTRUM,
  AUTO_PARTY,
  AUTO_EDM,
  AUTO_AFRO,
  AUTO_ROCK,
};

namespace {

int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

bool validFloat(float v) { return !isnan(v) && !isinf(v); }

ThemeDef makeTheme(const char* id, const char* name, const char* desc,
                   const Rgb* pal, int palCount, uint8_t brightPct,
                   uint8_t minBrightPct, uint8_t sat, float bassResp,
                   float lowMidResp, float midResp, float highMidResp,
                   float trebleResp, float beatResp, float ampResp,
                   float movement, float pulse, float beatFlash, float sparkle,
                   float smoothing, float contrast, float density,
                   const int* prefEffects, int prefCount, bool builtin) {
  ThemeDef t;
  strncpy(t.id, id, sizeof(t.id) - 1);
  strncpy(t.name, name, sizeof(t.name) - 1);
  if (desc) strncpy(t.description, desc, sizeof(t.description) - 1);
  for (int i = 0; i < palCount && i < Themes::kMaxThemeColors; ++i)
    t.palette[i] = pal[i];
  t.paletteCount = palCount;
  t.brightnessPct = brightPct;
  t.minBrightnessPct = minBrightPct;
  t.saturation = sat;
  t.bassResp = bassResp;
  t.lowMidResp = lowMidResp;
  t.midResp = midResp;
  t.highMidResp = highMidResp;
  t.trebleResp = trebleResp;
  t.beatResp = beatResp;
  t.ampResp = ampResp;
  t.movement = movement;
  t.pulse = pulse;
  t.beatFlash = beatFlash;
  t.sparkle = sparkle;
  t.smoothing = smoothing;
  t.contrast = contrast;
  t.density = density;
  for (int i = 0; i < prefCount && i < Themes::kMaxPrefEffect; ++i)
    t.preferredEffects[i] = prefEffects[i];
  t.prefEffectCount = ((int)prefCount < Themes::kMaxPrefEffect)
                          ? prefCount
                          : Themes::kMaxPrefEffect;
  t.builtin = builtin;
  return t;
}

// ---------------------------------------------------------------- palettes
const Rgb kRockPal[] = {{255, 0, 0}, {255, 58, 0}, {255, 119, 0}, {255, 255, 255}};
const Rgb kEdmPal[] = {{255, 0, 255}, {0, 255, 255}, {0, 102, 255}};
const Rgb kAmbientPal[] = {{27, 42, 91}, {58, 166, 185}, {125, 216, 200}};
const Rgb kPartyPal[] = {{255, 0, 64}, {255, 212, 0}, {0, 228, 255}, {138, 43, 226}};
const Rgb kChillPal[] = {{43, 91, 132}, {79, 163, 216}, {232, 184, 214}};
const Rgb kSpectrumPal[] = {{255, 0, 0}, {255, 122, 0}, {255, 212, 0},
                            {0, 200, 50}, {0, 200, 232}, {0, 80, 255},
                            {160, 0, 255}};
const Rgb kFirePal[] = {{255, 0, 0}, {255, 74, 0}, {255, 162, 0}, {255, 226, 0}};
const Rgb kOceanPal[] = {{0, 51, 102}, {0, 119, 170}, {0, 194, 209}, {91, 50, 160}};
const Rgb kCyberPal[] = {{255, 0, 224}, {0, 240, 255}, {90, 0, 255}, {0, 68, 255}};
const Rgb kBassPal[] = {{120, 0, 255}, {0, 80, 255}, {24, 20, 64}, {255, 255, 255}};
const Rgb kClubPal[] = {{255, 0, 96}, {180, 0, 255}, {0, 120, 255}, {255, 255, 255}};
const Rgb kVocalPal[] = {{212, 0, 255}, {0, 180, 255}, {38, 38, 96}};
const Rgb kBeatPal[] = {{255, 255, 255}, {0, 200, 255}, {255, 0, 180}};
const Rgb kClassicalPal[] = {{216, 140, 20}, {216, 180, 90}, {250, 245, 225}};
const Rgb kAfroPal[] = {{255, 122, 24}, {139, 58, 22}, {31, 168, 160},
                        {255, 209, 102}, {74, 30, 110}};

// AUTO mode stage palettes/params
const Rgb kAutoAmbientPal[] = {{20, 40, 90}, {50, 140, 180}, {90, 200, 220}};
const Rgb kAutoChillPal[] = {{60, 110, 160}, {120, 190, 230}, {220, 190, 220}};
const Rgb kAutoSpectrumPal[] = {{255, 40, 20}, {255, 170, 0}, {120, 255, 60},
                                {0, 210, 255}, {80, 90, 255}};
const Rgb kAutoPartyPal[] = {{255, 30, 90}, {255, 220, 0}, {0, 220, 255},
                             {150, 40, 255}};
const Rgb kAutoEdmPal[] = {{255, 0, 240}, {0, 255, 245}, {60, 120, 255}};

}  // namespace

ThemeEngine& ThemeEngine::instance() {
  static ThemeEngine engine;
  return engine;
}

void ThemeEngine::begin(Config& cfg) {
  _cfgPtr = &cfg;
  _mutex = xSemaphoreCreateMutex();
  _count = 0;

  if (!loadAll()) installDefaults();

  const char* global = cfg.themeId[0] ? cfg.themeId
                                      : Themes::kDefaultThemeId;
  setActiveRaw(kMaxStrips, resolve(global) ? global
                                           : Themes::kDefaultThemeId);
  for (int i = 0; i < kMaxStrips; ++i) {
    const char* id = cfg.strips[i].themeId;
    setActiveRaw(i, (id[0] && resolve(id)) ? id : "");
  }
}

// ------------------------------------------------------------ default themes
void ThemeEngine::installDefaults() {
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    _count = 0;

    // Order of arguments mirrors makeTheme(); see header for field meaning.
    _themes[_count++] =
        makeTheme("classic", "Classic", "Balanced neutral look", nullptr, 0,
                  100, 15, 255, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 1.0f,
                  0.5f, 0.5f, 0.25f, 0.3f, 0.3f, 0.5f, 0.6f,
                  (const int[]) {EFFECT_RUNNING_WAVE, EFFECT_MUSIC_WAVE,
                                 EFFECT_SPECTRUM},
                  3, true);

    _themes[_count++] =
        makeTheme("rock", "Rock", "Aggressive, punchy mids", kRockPal, 4, 100,
                  20, 250, 1.0f, 0.8f, 0.7f, 0.6f, 0.7f, 1.0f, 0.9f, 0.75f,
                  0.95f, 0.35f, 0.5f, 0.2f, 0.65f, 0.7f,
                  (const int[]) {EFFECT_BASS_PULSE, EFFECT_BEAT_RIPPLE,
                                 EFFECT_RUNNING_WAVE, EFFECT_SPARK},
                  4, true);
    _themes[_count++] =
        makeTheme("edm", "EDM", "High-energy, strong flashes", kEdmPal, 3, 100,
                  20, 255, 1.0f, 0.8f, 0.8f, 0.9f, 0.9f, 1.0f, 1.0f, 0.9f,
                  1.0f, 0.7f, 0.8f, 0.25f, 0.8f, 0.9f,
                  (const int[]) {EFFECT_BASS_PULSE, EFFECT_BEAT_FLASH,
                                 EFFECT_SPECTRUM, EFFECT_RUNNING_WAVE,
                                 EFFECT_SPARK},
                  5, true);
    _themes[_count++] =
        makeTheme("ambient", "Ambient", "Slow, relaxed breathing",
                  kAmbientPal, 3, 55, 8, 170, 0.35f, 0.6f, 0.55f, 0.35f, 0.35f,
                  0.15f, 0.6f, 0.15f, 0.12f, 0.05f, 0.05f, 0.8f, 0.35f, 0.3f,
                  (const int[]) {EFFECT_MUSIC_WAVE, EFFECT_ENERGY_PULSE,
                                 EFFECT_GRADIENT},
                  3, true);
    _themes[_count++] =
        makeTheme("party", "Party", "Bright multi-colour crowd", kPartyPal, 4,
                  100, 15, 255, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 1.0f, 1.0f, 0.7f,
                  1.0f, 0.6f, 0.8f, 0.2f, 0.7f, 0.9f,
                  (const int[]) {EFFECT_RAINBOW, EFFECT_SPECTRUM,
                                 EFFECT_RUNNING_WAVE, EFFECT_SPARK},
                  4, true);
    _themes[_count++] =
        makeTheme("chill", "Chill", "Relaxed warm glow", kChillPal, 3, 70, 10,
                  200, 0.45f, 0.55f, 0.5f, 0.4f, 0.4f, 0.3f, 0.7f, 0.28f,
                  0.22f, 0.08f, 0.1f, 0.65f, 0.4f, 0.4f,
                  (const int[]) {EFFECT_MUSIC_WAVE, EFFECT_ENERGY_PULSE}, 2,
                  true);
    _themes[_count++] =
        makeTheme("spectrum", "Spectrum", "Frequency mapped to colour",
                  kSpectrumPal, 7, 100, 15, 255, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f,
                  0.8f, 1.0f, 0.45f, 0.55f, 0.2f, 0.45f, 0.35f, 0.75f, 0.8f,
                  (const int[]) {EFFECT_SPECTRUM, EFFECT_FREQ_WAVE,
                                 EFFECT_GRADIENT},
                  3, true);
    _themes[_count++] =
        makeTheme("fire", "Fire", "Audio-driven flames", kFirePal, 4, 100, 20,
                  255, 1.0f, 0.7f, 0.55f, 0.45f, 0.4f, 0.7f, 0.95f, 0.6f, 0.5f,
                  0.15f, 0.6f, 0.25f, 0.7f, 0.7f,
                  (const int[]) {EFFECT_COLOR_ENERGY, EFFECT_GRADIENT,
                                 EFFECT_SPARK},
                  3, true);
    _themes[_count++] =
        makeTheme("ocean", "Ocean", "Slow underwater waves", kOceanPal, 4, 75,
                  12, 230, 0.4f, 0.5f, 0.55f, 0.5f, 0.4f, 0.2f, 0.65f, 0.3f,
                  0.2f, 0.05f, 0.08f, 0.7f, 0.35f, 0.45f,
                  (const int[]) {EFFECT_MUSIC_WAVE, EFFECT_GRADIENT,
                                 EFFECT_FREQ_WAVE},
                  3, true);
    _themes[_count++] =
        makeTheme("cyberpunk", "Cyberpunk", "Sharp electronic contrast",
                  kCyberPal, 4, 100, 18, 255, 0.95f, 0.85f, 0.8f, 0.9f, 0.9f,
                  1.0f, 1.0f, 0.85f, 0.9f, 0.55f, 0.9f, 0.15f, 0.85f, 0.85f,
                  (const int[]) {EFFECT_RUNNING_WAVE, EFFECT_BEAT_RIPPLE,
                                 EFFECT_SPARK, EFFECT_SPECTRUM},
                  4, true);
    _themes[_count++] =
        makeTheme("bass_heavy", "Bass Heavy", "Deep sub-driven power",
                  kBassPal, 4, 95, 18, 240, 1.0f, 0.85f, 0.5f, 0.4f, 0.45f,
                  0.9f, 0.9f, 0.5f, 0.9f, 0.25f, 0.3f, 0.3f, 0.75f, 0.85f,
                  (const int[]) {EFFECT_BASS_PULSE, EFFECT_SPECTRUM,
                                 EFFECT_ENERGY_PULSE},
                  3, true);
    _themes[_count++] =
        makeTheme("rainbow", "Rainbow", "Continuous spectrum cycling", nullptr,
                  0, 100, 15, 255, 0.8f, 0.8f, 0.85f, 0.8f, 0.7f, 0.85f, 1.0f,
                  0.9f, 0.7f, 0.4f, 0.6f, 0.2f, 0.6f, 0.85f,
                  (const int[]) {EFFECT_RAINBOW, EFFECT_RUNNING_WAVE,
                                 EFFECT_SPECTRUM},
                  3, true);
    _themes[_count++] =
        makeTheme("club", "Club", "Drive-heavy dance floor", kClubPal, 4, 100,
                  20, 255, 1.0f, 0.9f, 0.7f, 0.8f, 0.8f, 1.0f, 1.0f, 0.8f,
                  0.95f, 0.5f, 0.7f, 0.2f, 0.75f, 0.9f,
                  (const int[]) {EFFECT_BASS_PULSE, EFFECT_SPECTRUM,
                                 EFFECT_RUNNING_WAVE, EFFECT_ENERGY_PULSE},
                  4, true);
    _themes[_count++] =
        makeTheme("vocal", "Vocal", "Focused on the voice", kVocalPal, 3, 90,
                  14, 240, 0.5f, 0.5f, 0.95f, 1.0f, 0.95f, 0.5f, 0.85f, 0.35f,
                  0.4f, 0.2f, 0.5f, 0.3f, 0.6f, 0.7f,
                  (const int[]) {EFFECT_FREQ_WAVE, EFFECT_SPECTRUM,
                                 EFFECT_SPARK},
                  3, true);
    _themes[_count++] =
        makeTheme("beat", "Beat", "Hypnotic beat-locked pulses", kBeatPal, 3,
                  95, 10, 255, 0.9f, 0.7f, 0.5f, 0.5f, 0.6f, 1.0f, 0.85f, 0.3f,
                  1.0f, 0.4f, 0.4f, 0.25f, 0.8f, 0.75f,
                  (const int[]) {EFFECT_BEAT_RIPPLE, EFFECT_BEAT_FLASH,
                                 EFFECT_ENERGY_PULSE},
                  3, true);
    _themes[_count++] =
        makeTheme("classical", "Classical", "Elegant, gentle dynamics",
                  kClassicalPal, 3, 60, 8, 200, 0.4f, 0.45f, 0.6f, 0.5f, 0.45f,
                  0.2f, 0.7f, 0.2f, 0.15f, 0.03f, 0.08f, 0.75f, 0.4f, 0.4f,
                  (const int[]) {EFFECT_MUSIC_WAVE, EFFECT_GRADIENT,
                                 EFFECT_FREQ_WAVE},
                  3, true);
    _themes[_count++] =
        makeTheme(
            "afro_house", "Afro House", "Deep, warm, hypnotic groove",
            kAfroPal, 5, 50, 12, 235, 0.85f, 0.95f, 0.65f, 0.45f, 0.35f, 0.75f,
            0.85f, 0.55f, 0.55f, 0.15f, 0.2f, 0.65f, 0.55f, 0.7f,
            (const int[]) {EFFECT_RUNNING_WAVE, EFFECT_ENERGY_PULSE,
                           EFFECT_FREQ_WAVE, EFFECT_BASS_PULSE,
                           EFFECT_COLOR_ENERGY},
            5, true);
    _themes[_count++] =
        makeTheme("auto", "AUTO", "Music-driven adaptive behaviour", nullptr,
                  0, 90, 15, 240, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 1.0f, 1.0f,
                  0.5f, 0.6f, 0.3f, 0.5f, 0.4f, 0.6f, 0.7f,
                  (const int[]) {EFFECT_SPECTRUM, EFFECT_RUNNING_WAVE,
                                 EFFECT_ENERGY_PULSE},
                  3, true);

    xSemaphoreGive(_mutex);
  }
}

// ------------------------------------------------------------------ helpers
const ThemeDef* ThemeEngine::resolve(const char* id) const {
  if (!id || !id[0]) return nullptr;
  for (int i = 0; i < _count; ++i) {
    if (strncmp(_themes[i].id, id, sizeof(_themes[i].id)) == 0) return &_themes[i];
  }
  return nullptr;
}

const ThemeDef* ThemeEngine::find(const char* id) const {
  if (!_mutex) return nullptr;
  const ThemeDef* r = nullptr;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    r = resolve(id);
    xSemaphoreGive(_mutex);
  }
  return r;
}

const ThemeDef* ThemeEngine::get(int index) const {
  if (!_mutex || index < 0 || index >= _count) return nullptr;
  const ThemeDef* r = nullptr;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    r = (index < _count) ? &_themes[index] : nullptr;
    xSemaphoreGive(_mutex);
  }
  return r;
}

void ThemeEngine::setActiveRaw(int slot, const char* id) {
  if (!id) id = Themes::kDefaultThemeId;
  strncpy(_active[slot], id, sizeof(_active[slot]) - 1);
  _active[slot][sizeof(_active[slot]) - 1] = '\0';
}

const char* ThemeEngine::activeId() const {
  return _active[kMaxStrips][0] ? _active[kMaxStrips]
                                : Themes::kDefaultThemeId;
}

const char* ThemeEngine::activeStripId(int strip) const {
  if (strip < 0 || strip >= kMaxStrips) return "";
  return _active[strip];
}

bool ThemeEngine::apply(const char* id) {
  if (!_mutex || !_cfgPtr) return false;
  const ThemeDef* t = nullptr;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    t = resolve(id);
    if (t) {
      setActiveRaw(kMaxStrips, t->id);
      strncpy(_cfgPtr->themeId, t->id, sizeof(_cfgPtr->themeId) - 1);
      _cfgPtr->themeId[sizeof(_cfgPtr->themeId) - 1] = '\0';
    }
    xSemaphoreGive(_mutex);
  }
  if (!t) return false;
  return ConfigStore::save(*_cfgPtr);  // persist across reboot
}

bool ThemeEngine::applyStrip(int strip, const char* id) {
  if (!_mutex || !_cfgPtr || strip < 0 || strip >= kMaxStrips) return false;
  bool ok = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (id && id[0]) {
      if (resolve(id)) {
        setActiveRaw(strip, id);
        strncpy(_cfgPtr->strips[strip].themeId, id,
                sizeof(_cfgPtr->strips[strip].themeId) - 1);
        _cfgPtr->strips[strip]
            .themeId[sizeof(_cfgPtr->strips[strip].themeId) - 1] = '\0';
        ok = true;
      }
    } else {  // empty id clears the override -> follow global
      setActiveRaw(strip, "");
      _cfgPtr->strips[strip].themeId[0] = '\0';
      ok = true;
    }
    xSemaphoreGive(_mutex);
  }
  return ok && ConfigStore::save(*_cfgPtr);
}

// ---------------------------------------------------------------------- CRUD
bool ThemeEngine::update(const ThemeDef& t, bool upsert, char* err,
                         size_t errLen) {
  if (!_mutex) {
    snprintf(err, errLen, "engine uninitialized");
    return false;
  }
  if (!t.id[0]) {
    snprintf(err, errLen, "missing id");
    return false;
  }
  bool ok = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ThemeDef* existing = nullptr;
    for (int i = 0; i < _count; ++i)
      if (strcmp(_themes[i].id, t.id) == 0) existing = &_themes[i];

    if (!existing) {
      if (!upsert) {
        snprintf(err, errLen, "theme not found: %s", t.id);
      } else if (_count >= Themes::kMaxThemes) {
        snprintf(err, errLen, "max themes reached (%d)", Themes::kMaxThemes);
      } else {
        existing = &_themes[_count++];
      }
    }
    if (existing) {
      ThemeDef c = t;
      c.builtin = existing->builtin && c.builtin;  // builtins stay builtin
      *existing = c;
      ok = saveAllLocked();
    }
    xSemaphoreGive(_mutex);
  }
  if (!ok && err && errLen) snprintf(err, errLen, "write failed");
  return ok;
}

bool ThemeEngine::remove(const char* id, char* err, size_t errLen) {
  if (!_mutex) return false;
  bool ok = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    int idx = -1;
    for (int i = 0; i < _count; ++i)
      if (strcmp(_themes[i].id, id) == 0) idx = i;
    if (idx < 0) {
      snprintf(err, errLen, "theme not found: %s", id);
    } else if (_themes[idx].builtin) {
      snprintf(err, errLen, "%s is built-in", id);
    } else {
      for (int i = idx; i < _count - 1; ++i) _themes[i] = _themes[i + 1];
      _count--;
      ok = saveAllLocked();
    }
    xSemaphoreGive(_mutex);
  }
  return ok;
}

void ThemeEngine::resetDefaults() {
  installDefaults();
  saveAll();
}

// ------------------------------------------------------------------ JSON I/O
bool ThemeEngine::parseHex(const char* hex, Rgb& out) {
  if (!hex) return false;
  while (*hex == '#') ++hex;
  if (strlen(hex) < 6) return false;
  unsigned int r, g, b;
  if (sscanf(hex, "%2x%2x%2x", &r, &g, &b) != 3) return false;
  out.r = r;
  out.g = g;
  out.b = b;
  return true;
}

const char* ThemeEngine::hexColor(const Rgb& c, char* buf) {
  snprintf(buf, 8, "#%02X%02X%02X", c.r, c.g, c.b);
  return buf;
}

bool ThemeEngine::decode(const JsonVariantConst& src, ThemeDef& out, char* err,
                         size_t errLen) {
  const char* id = src["id"] | "";
  const char* name = src["name"] | id;
  if (!id[0]) {
    snprintf(err, errLen, "theme requires an id");
    return false;
  }
  out = ThemeDef();
  strncpy(out.id, id, sizeof(out.id) - 1);
  strncpy(out.name, name, sizeof(out.name) - 1);
  const char* desc = src["description"] | "";
  if (desc[0]) strncpy(out.description, desc, sizeof(out.description) - 1);

  // palette: either an array of "#RRGGBB" strings, or an object with the
  // named slots {primary, secondary, accent, background} — both supported.
  JsonVariantConst pal = src["palette"];
  if (pal.is<JsonArrayConst>()) {
    int n = 0;
    for (JsonVariantConst c : pal.as<JsonArrayConst>()) {
      if (n >= Themes::kMaxThemeColors) break;
      Rgb rgb;
      if (!parseHex(c.as<const char*>(), rgb)) {
        snprintf(err, errLen, "bad colour %s", c.as<const char*>());
        return false;
      }
      out.palette[n++] = rgb;
    }
    out.paletteCount = n;
  } else if (pal.is<JsonObjectConst>()) {
    const char* slots[Themes::kMaxThemeColors] = {"primary", "secondary",
                                                  "accent",  "background"};
    int n = 0;
    for (int i = 0; i < 4; ++i) {
      const char* hex = pal[slots[i]] | "";
      if (!hex[0]) continue;
      Rgb rgb;
      if (!parseHex(hex, rgb)) {
        snprintf(err, errLen, "bad colour %s", hex);
        return false;
      }
      out.palette[n++] = rgb;
    }
    out.paletteCount = n;
  }

  // brightness: scalar (0..100) for back-compat, or {"base","min"} object.
  JsonVariantConst br = src["brightness"];
  if (br.is<JsonObjectConst>()) {
    out.brightnessPct = clampInt((int)(br["base"] | out.brightnessPct), 0, 100);
    out.minBrightnessPct =
        clampInt((int)(br["min"] | out.minBrightnessPct), 0, 100);
    if (out.minBrightnessPct > out.brightnessPct)
      out.minBrightnessPct = out.brightnessPct;
  } else {
    out.brightnessPct = clampInt((int)(br | out.brightnessPct), 0, 100);
    out.minBrightnessPct = clampInt((int)(src["minBrightness"] |
                                          out.minBrightnessPct), 0, 100);
    if (out.minBrightnessPct > out.brightnessPct)
      out.minBrightnessPct = out.brightnessPct;
  }
  out.saturation = clampInt((int)(src["saturation"] | out.saturation), 0, 255);

  JsonVariantConst r = src["response"];
  if (!r.isNull()) {
    out.bassResp = validFloat(r["bass"]) ? clampFloat(r["bass"].as<float>(), 0.0f, 2.0f) : out.bassResp;
    out.lowMidResp = validFloat(r["lowMid"]) ? clampFloat(r["lowMid"].as<float>(), 0.0f, 2.0f) : out.lowMidResp;
    out.midResp = validFloat(r["mid"]) ? clampFloat(r["mid"].as<float>(), 0.0f, 2.0f) : out.midResp;
    out.highMidResp = validFloat(r["highMid"]) ? clampFloat(r["highMid"].as<float>(), 0.0f, 2.0f) : out.highMidResp;
    out.trebleResp = validFloat(r["treble"]) ? clampFloat(r["treble"].as<float>(), 0.0f, 2.0f) : out.trebleResp;
    out.beatResp = validFloat(r["beat"]) ? clampFloat(r["beat"].as<float>(), 0.0f, 2.0f) : out.beatResp;
    out.ampResp = validFloat(r["amp"]) ? clampFloat(r["amp"].as<float>(), 0.0f, 2.0f) : out.ampResp;
  }
  JsonVariantConst an = src["animation"];
  if (!an.isNull()) {
    out.movement = validFloat(an["movement"]) ? clampFloat(an["movement"].as<float>(), 0.0f, 1.0f) : out.movement;
    out.pulse = validFloat(an["pulse"]) ? clampFloat(an["pulse"].as<float>(), 0.0f, 1.0f) : out.pulse;
    out.beatFlash = validFloat(an["flash"]) ? clampFloat(an["flash"].as<float>(), 0.0f, 1.0f) : out.beatFlash;
    out.sparkle = validFloat(an["sparkle"]) ? clampFloat(an["sparkle"].as<float>(), 0.0f, 1.0f) : out.sparkle;
    out.smoothing = validFloat(an["smoothing"]) ? clampFloat(an["smoothing"].as<float>(), 0.0f, 1.0f) : out.smoothing;
    out.contrast = validFloat(an["contrast"]) ? clampFloat(an["contrast"].as<float>(), 0.0f, 1.0f) : out.contrast;
    out.density = validFloat(an["density"]) ? clampFloat(an["density"].as<float>(), 0.0f, 1.0f) : out.density;
  }
  JsonVariantConst fx = src["effects"];
  if (fx.is<JsonArrayConst>()) {
    int n = 0;
    for (JsonVariantConst e : fx.as<JsonArrayConst>()) {
      if (n >= Themes::kMaxPrefEffect) break;
      int v = e.as<int>();
      if (v < 0 || v >= EFFECT_COUNT) continue;
      out.preferredEffects[n++] = v;
    }
    out.prefEffectCount = n;
  }
  return true;
}

void ThemeEngine::encode(const ThemeDef& t, JsonObject obj) {
  obj["id"] = t.id;
  obj["name"] = t.name;
  if (t.description[0]) obj["description"] = t.description;
  obj["builtin"] = t.builtin;
  JsonObject br = obj["brightness"].to<JsonObject>();
  br["base"] = t.brightnessPct;
  br["min"] = t.minBrightnessPct;
  obj["saturation"] = t.saturation;
  JsonArray pal = obj["palette"].to<JsonArray>();
  char buf[8];
  for (int i = 0; i < t.paletteCount; ++i) pal.add(hexColor(t.palette[i], buf));
  JsonObject r = obj["response"].to<JsonObject>();
  r["bass"] = t.bassResp;
  r["lowMid"] = t.lowMidResp;
  r["mid"] = t.midResp;
  r["highMid"] = t.highMidResp;
  r["treble"] = t.trebleResp;
  r["beat"] = t.beatResp;
  r["amp"] = t.ampResp;
  JsonObject an = obj["animation"].to<JsonObject>();
  an["movement"] = t.movement;
  an["pulse"] = t.pulse;
  an["flash"] = t.beatFlash;
  an["sparkle"] = t.sparkle;
  an["smoothing"] = t.smoothing;
  an["contrast"] = t.contrast;
  an["density"] = t.density;
  JsonArray ef = obj["effects"].to<JsonArray>();
  for (int i = 0; i < t.prefEffectCount; ++i)
    ef.add(t.preferredEffects[i]);
}

bool ThemeEngine::saveAllLocked() {
  File f = LittleFS.open(kThemesPath, "w");
  if (!f) return false;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < _count; ++i) {
    JsonObject o = arr.add<JsonObject>();
    encode(_themes[i], o);
  }
  size_t w = serializeJson(doc, f);
  f.close();
  return w > 0;
}

bool ThemeEngine::saveAll() {
  if (!_mutex) return false;
  bool ok = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ok = saveAllLocked();
    xSemaphoreGive(_mutex);
  }
  return ok;
}

bool ThemeEngine::loadAll() {
  if (!LittleFS.exists(kThemesPath)) return false;
  File f = LittleFS.open(kThemesPath, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  if (!doc.is<JsonArrayConst>()) return false;

  _count = 0;
  for (JsonVariantConst v : doc.as<JsonArrayConst>()) {
    if (_count >= Themes::kMaxThemes) break;
    char errBuf[64];
    if (!decode(v, _themes[_count], errBuf, sizeof(errBuf))) continue;
    _themes[_count].builtin = false;  // file never creates builtins
    _count++;
  }
  return _count > 0;
}

// ------------------------------------------------------------------ evaluation
ThemeFrame ThemeEngine::processTheme(const ThemeDef& def, const AudioFrame& a,
                                     uint32_t nowMs) {
  (void)nowMs;
  ThemeFrame f;
  const float beatK = a.beat ? 1.0f : 0.0f;
  f.bassResponse = clampFloat(a.bass * def.bassResp, 0.0f, 1.0f);
  f.lowMidResponse = clampFloat(a.lowMid * def.lowMidResp, 0.0f, 1.0f);
  f.midResponse = clampFloat(a.mid * def.midResp, 0.0f, 1.0f);
  f.highMidResponse = clampFloat(a.highMid * def.highMidResp, 0.0f, 1.0f);
  f.trebleResponse = clampFloat(a.treble * def.trebleResp, 0.0f, 1.0f);
  f.intensity =
      clampFloat(a.amplitude * def.ampResp + beatK * def.beatResp * 0.25f, 0.0f,
                 1.0f);
  f.energy = f.intensity;
  f.saturation =
      clampFloat(def.saturation / 255.0f + beatK * def.beatResp * 0.10f, 0.0f,
                 1.0f);
  // base brightness floor (min) .. ceiling (base), energy-stretched by
  // contrast: high contrast themes breathe harder between floor and ceiling.
  const float base01 = def.brightnessPct / 100.0f;
  const float min01 = def.minBrightnessPct / 100.0f;
  float bright01 = base01 * (0.55f + 0.45f * f.intensity);
  const float lowKnob = min01 + (base01 - min01) * (1.0f - def.contrast);
  if (lowKnob > bright01) bright01 = lowKnob;  // contrast lifts the floor
  f.brightness = clampFloat(bright01, 0.0f, 1.0f);
  // motion: base speed pulled by audio; low-mid groove adds rolling motion
  // without necessarily raising raw energy (the Afro House signature).
  f.movement = clampFloat(def.movement *
                             (0.35f + 0.55f * f.bassResponse +
                              0.35f * f.lowMidResponse),
                         0.0f, 1.0f);
  f.pulse = clampFloat(def.pulse * (0.25f + beatK * 0.75f), 0.0f, 1.0f);
  f.beatFlash = clampFloat(def.beatFlash * beatK * def.beatResp, 0.0f, 1.0f);
  f.sparkle = clampFloat(def.sparkle * f.trebleResponse, 0.0f, 1.0f);
  f.colourShift =
      clampFloat(def.midResp * clampFloat(a.mid, 0.0f, 1.0f) * 0.10f +
                     def.lowMidResp * clampFloat(a.lowMid, 0.0f, 1.0f) * 0.05f +
                     beatK * def.beatResp * 0.06f,
                 0.0f, 1.0f);
  f.beatResponse = clampFloat(beatK * def.beatResp, 0.0f, 1.0f);
  f.smoothing = clampFloat(def.smoothing, 0.0f, 1.0f);
  f.contrast = clampFloat(def.contrast, 0.0f, 1.0f);
  f.density = clampFloat(def.density, 0.0f, 1.0f);
  f.transitionSpeed = 1.0f - f.smoothing;
  f.palette = (def.paletteCount > 0) ? def.palette : nullptr;
  f.paletteCount = def.paletteCount;
  if (def.paletteCount > 0) {
    f.primary = def.palette[0];
    if (def.paletteCount > 1) f.secondary = def.palette[1];
    if (def.paletteCount > 2) f.accent = def.palette[2];
    f.background = def.palette[def.paletteCount - 1];
  }
  f.preferredEffect =
      (def.prefEffectCount > 0) ? def.preferredEffects[0] : -1;
  f.themeId = def.id;
  f.themeName = def.name;
  return f;
}

// AUTO: one theme that adapts its own parameters (hysteresis + min duration).
//
// Classifier: two feature classes (groove/afro, aggressive-mid/rock) vote on
// top of an energy ladder. Each class must hold above its vote threshold for a
// couple of seconds, and any switch requires a minimum theme dwell time, so
// the visuals never flicker between personalities.
ThemeFrame ThemeEngine::processAuto(const AudioFrame& a, uint32_t nowMs) {
  const float attack = 0.25f;
  const float release = 0.12f;
  const float in = clampFloat(a.amplitude, 0.0f, 1.0f);
  _autoEnergy += (in - _autoEnergy) * (in > _autoEnergy ? attack : release);
  const float e = _autoEnergy;

  // Groove energy: low-mid percussion riding on a bass foundation. Strong
  // low-mid + low treble + moderate energy = Afro House character.
  const float grooveIn =
      clampFloat(a.lowMid * (0.4f + 0.6f * a.bass), 0.0f, 1.0f);
  _autoGroove += (grooveIn - _autoGroove) *
                 (grooveIn > _autoGroove ? 0.18f : 0.08f);
  const float g = _autoGroove;

  const uint32_t dt =
      _autoLastMs ? (nowMs >= _autoLastMs ? nowMs - _autoLastMs : 16u) : 16u;
  _autoLastMs = nowMs;

  const bool voteAfro = g > 0.45f && a.bass > 0.25f && e > 0.30f &&
                        e < 0.75f && a.treble < 0.55f && a.mid < g + 0.05f;
  const bool voteRock = e > 0.55f && a.mid > 0.50f && a.bass > 0.50f &&
                        a.treble > 0.25f;
  _autoAfroMs = voteAfro ? (_autoAfroMs + dt) : 0u;
  _autoRockMs = voteRock ? (_autoRockMs + dt) : 0u;

  const uint32_t kVoteMs = 2000;          // sustained feature vote
  const uint32_t kMinHoldMs = 4000;       // minimum theme dwell time
  const bool held = (nowMs - _autoStageSince) >= kMinHoldMs;

  int next = _autoStage;
  if (held) {
    if (_autoAfroMs >= kVoteMs) {
      next = AUTO_AFRO;
    } else if (_autoRockMs >= kVoteMs) {
      next = AUTO_ROCK;
    } else {
      // energy ladder with hysteresis
      static const float kLo[] = {0.20f, 0.34f, 0.50f, 0.66f};
      static const float kHi[] = {0.30f, 0.44f, 0.60f, 0.76f};
      int stage = _autoStage;
      if (stage > AUTO_EDM) stage = AUTO_EDM;  // class stages fall back to EDM
      if (e < kLo[stage]) {
        next = stage > AUTO_AMBIENT ? stage - 1 : AUTO_AMBIENT;
      } else if (e > kHi[stage]) {
        next = stage < AUTO_EDM ? stage + 1 : AUTO_EDM;
      } else {
        next = stage;
      }
    }
  }
  if (next != _autoStage && held) {
    _autoStage = next;
    _autoStageSince = nowMs;
  }

  ThemeFrame f;
  const Rgb* pal = nullptr;
  int palCount = 0;
  uint8_t bright = 80, sat = 220;
  f.movement = 0.35f;
  f.pulse = 0.4f;
  f.beatFlash = 0.2f;
  f.sparkle = 0.4f;
  f.smoothing = 0.4f;
  f.contrast = 0.6f;
  f.density = 0.7f;
  const char* stageName = "Auto";

  switch (_autoStage) {
    case AUTO_AMBIENT:
      pal = kAutoAmbientPal; palCount = 3; bright = 45; sat = 170;
      f.movement = 0.12f; f.pulse = 0.10f; f.beatFlash = 0.02f;
      f.sparkle = 0.05f; f.smoothing = 0.75f; f.contrast = 0.35f; f.density = 0.3f;
      stageName = "Ambient";
      break;
    case AUTO_CHILL:
      pal = kAutoChillPal; palCount = 3; bright = 62; sat = 200;
      f.movement = 0.22f; f.pulse = 0.22f; f.beatFlash = 0.05f;
      f.sparkle = 0.12f; f.smoothing = 0.6f; f.contrast = 0.4f; f.density = 0.4f;
      stageName = "Chill";
      break;
    case AUTO_SPECTRUM:
      pal = kAutoSpectrumPal; palCount = 5; bright = 82; sat = 230;
      f.movement = 0.38f; f.pulse = 0.45f; f.beatFlash = 0.12f;
      f.sparkle = 0.45f; f.smoothing = 0.35f; f.contrast = 0.75f; f.density = 0.8f;
      stageName = "Spectrum";
      break;
    case AUTO_PARTY:
      pal = kAutoPartyPal; palCount = 4; bright = 95; sat = 255;
      f.movement = 0.6f; f.pulse = 0.85f; f.beatFlash = 0.45f;
      f.sparkle = 0.65f; f.smoothing = 0.22f; f.contrast = 0.7f; f.density = 0.9f;
      stageName = "Party";
      break;
    case AUTO_EDM:
      pal = kAutoEdmPal; palCount = 3; bright = 100; sat = 255;
      f.movement = 0.9f; f.pulse = 1.0f; f.beatFlash = 0.6f;
      f.sparkle = 0.9f; f.smoothing = 0.12f; f.contrast = 0.8f; f.density = 0.9f;
      stageName = "EDM";
      break;
    case AUTO_AFRO:
      pal = kAfroPal; palCount = 5; bright = 55; sat = 235;
      f.movement = 0.55f; f.pulse = 0.55f; f.beatFlash = 0.1f;
      f.sparkle = 0.2f; f.smoothing = 0.6f; f.contrast = 0.55f; f.density = 0.7f;
      stageName = "Afro House";
      break;
    case AUTO_ROCK:
      pal = kRockPal; palCount = 4; bright = 100; sat = 250;
      f.movement = 0.75f; f.pulse = 0.95f; f.beatFlash = 0.3f;
      f.sparkle = 0.5f; f.smoothing = 0.18f; f.contrast = 0.65f; f.density = 0.7f;
      stageName = "Rock";
      break;
  }

  const float beatK = a.beat ? 1.0f : 0.0f;
  f.bassResponse = clampFloat(a.bass, 0.0f, 1.0f);
  f.lowMidResponse = clampFloat(a.lowMid, 0.0f, 1.0f);
  f.midResponse = clampFloat(a.mid, 0.0f, 1.0f);
  f.highMidResponse = clampFloat(a.highMid, 0.0f, 1.0f);
  f.trebleResponse = clampFloat(a.treble, 0.0f, 1.0f);
  f.intensity = e;
  f.energy = e;
  f.saturation = sat / 255.0f + beatK * 0.12f;
  if (f.saturation > 1.0f) f.saturation = 1.0f;
  f.brightness = clampFloat((bright / 100.0f) * (0.55f + 0.45f * e), 0.0f, 1.0f);
  f.pulse = clampFloat(f.pulse * (0.25f + beatK * 0.75f), 0.0f, 1.0f);
  f.beatFlash = clampFloat(f.beatFlash * beatK, 0.0f, 1.0f);
  f.sparkle = clampFloat(f.sparkle * f.trebleResponse, 0.0f, 1.0f);
  f.colourShift = beatK * 0.05f;
  f.beatResponse = clampFloat(beatK, 0.0f, 1.0f);
  f.transitionSpeed = 1.0f - f.smoothing;
  f.palette = pal;
  f.paletteCount = palCount;
  if (palCount > 0) {
    f.primary = pal[0];
    if (palCount > 1) f.secondary = pal[1];
    if (palCount > 2) f.accent = pal[2];
    f.background = pal[palCount - 1];
  }
  f.preferredEffect = -1;
  f.themeId = Themes::kAutoThemeId;
  f.themeName = stageName;
  return f;
}

ThemeFrame ThemeEngine::process(const AudioFrame& a, uint32_t nowMs) {
  if (!_mutex) return identityFrame();
  bool autoMode = false;
  bool have = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    const char* id = activeId();
    if (strncmp(id, Themes::kAutoThemeId, sizeof(Themes::kAutoThemeId)) == 0) {
      autoMode = true;
    } else {
      const ThemeDef* p = resolve(id);
      if (p) {
        _work[kMaxStrips] = *p;  // copy under lock; palette points into _work
        have = true;
      }
    }
    xSemaphoreGive(_mutex);
  }
  if (autoMode) return processAuto(a, nowMs);
  return have ? processTheme(_work[kMaxStrips], a, nowMs) : identityFrame();
}

ThemeFrame ThemeEngine::processStrip(int strip, const AudioFrame& a,
                                     uint32_t nowMs) {
  if (!_mutex || strip < 0 || strip >= kMaxStrips) return identityFrame();
  bool autoMode = false;
  bool have = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    const char* id = _active[strip][0] ? _active[strip] : activeId();
    if (strncmp(id, Themes::kAutoThemeId, sizeof(Themes::kAutoThemeId)) == 0) {
      autoMode = true;
    } else {
      const ThemeDef* p = resolve(id);
      if (p) {
        _work[strip] = *p;
        have = true;
      }
    }
    xSemaphoreGive(_mutex);
  }
  if (autoMode) return processAuto(a, nowMs);
  return have ? processTheme(_work[strip], a, nowMs) : identityFrame();
}