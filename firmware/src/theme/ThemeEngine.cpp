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

ThemeDef makeTheme(const char* id, const char* name, const Rgb* pal,
                   int palCount, uint8_t brightPct, uint8_t sat, float bassResp,
                   float midResp, float trebleResp, float beatResp,
                   float ampResp, float movement, float pulse, float sparkle,
                   float smoothing, bool builtin) {
  ThemeDef t;
  strncpy(t.id, id, sizeof(t.id) - 1);
  strncpy(t.name, name, sizeof(t.name) - 1);
  for (int i = 0; i < palCount && i < Themes::kMaxThemeColors; ++i)
    t.palette[i] = pal[i];
  t.paletteCount = palCount;
  t.brightnessPct = brightPct;
  t.saturation = sat;
  t.bassResp = bassResp;
  t.midResp = midResp;
  t.trebleResp = trebleResp;
  t.beatResp = beatResp;
  t.ampResp = ampResp;
  t.movement = movement;
  t.pulse = pulse;
  t.sparkle = sparkle;
  t.smoothing = smoothing;
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
    _themes[_count++] = makeTheme("classic", "Classic", nullptr, 0, 100, 255,
                                  0.35f, 0.35f, 0.35f, 0.35f, 1.0f, 0.5f, 0.5f,
                                  0.3f, 0.3f, true);

    _themes[_count++] =
        makeTheme("rock", "Rock", kRockPal, 4, 100, 250, 1.0f, 0.55f, 0.7f,
                  1.0f, 0.9f, 0.75f, 0.95f, 0.5f, 0.2f, true);
    _themes[_count++] =
        makeTheme("edm", "EDM", kEdmPal, 3, 100, 255, 1.0f, 0.8f, 0.9f, 1.0f,
                  1.0f, 0.9f, 1.0f, 0.8f, 0.25f, true);
    _themes[_count++] =
        makeTheme("ambient", "Ambient", kAmbientPal, 3, 55, 170, 0.35f, 0.6f,
                  0.4f, 0.15f, 0.6f, 0.15f, 0.12f, 0.05f, 0.8f, true);
    _themes[_count++] =
        makeTheme("party", "Party", kPartyPal, 4, 100, 255, 0.9f, 0.9f, 0.9f,
                  1.0f, 1.0f, 0.7f, 1.0f, 0.8f, 0.2f, true);
    _themes[_count++] =
        makeTheme("chill", "Chill", kChillPal, 3, 70, 200, 0.45f, 0.55f, 0.5f,
                  0.3f, 0.7f, 0.28f, 0.22f, 0.1f, 0.65f, true);
    _themes[_count++] =
        makeTheme("spectrum", "Spectrum", kSpectrumPal, 7, 100, 255, 0.9f, 0.9f,
                  0.9f, 0.8f, 1.0f, 0.45f, 0.55f, 0.45f, 0.35f, true);
    _themes[_count++] =
        makeTheme("fire", "Fire", kFirePal, 4, 100, 255, 1.0f, 0.6f, 0.4f,
                  0.7f, 0.95f, 0.6f, 0.5f, 0.6f, 0.25f, true);
    _themes[_count++] =
        makeTheme("ocean", "Ocean", kOceanPal, 4, 75, 230, 0.4f, 0.5f, 0.5f,
                  0.2f, 0.65f, 0.3f, 0.2f, 0.08f, 0.7f, true);
    _themes[_count++] =
        makeTheme("cyberpunk", "Cyberpunk", kCyberPal, 4, 100, 255, 0.95f, 0.8f,
                  0.9f, 1.0f, 1.0f, 0.85f, 0.9f, 0.9f, 0.15f, true);
    _themes[_count++] =
        makeTheme("auto", "AUTO", nullptr, 0, 90, 240, 0.8f, 0.8f, 0.8f, 1.0f,
                  1.0f, 0.5f, 0.6f, 0.5f, 0.4f, true);

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

  if (src["palette"].is<JsonArrayConst>()) {
    int n = 0;
    for (JsonVariantConst c : src["palette"].as<JsonArrayConst>()) {
      if (n >= Themes::kMaxThemeColors) break;
      Rgb rgb;
      if (!parseHex(c.as<const char*>(), rgb)) {
        snprintf(err, errLen, "bad colour %s", c.as<const char*>());
        return false;
      }
      out.palette[n++] = rgb;
    }
    out.paletteCount = n;
  }

  out.brightnessPct = clampInt((int)(src["brightness"] | out.brightnessPct), 0, 100);
  out.saturation = clampInt((int)(src["saturation"] | out.saturation), 0, 255);

  JsonVariantConst r = src["response"];
  if (!r.isNull()) {
    out.bassResp = validFloat(r["bass"]) ? clampFloat(r["bass"].as<float>(), 0.0f, 2.0f) : out.bassResp;
    out.midResp = validFloat(r["mid"]) ? clampFloat(r["mid"].as<float>(), 0.0f, 2.0f) : out.midResp;
    out.trebleResp = validFloat(r["treble"]) ? clampFloat(r["treble"].as<float>(), 0.0f, 2.0f) : out.trebleResp;
    out.beatResp = validFloat(r["beat"]) ? clampFloat(r["beat"].as<float>(), 0.0f, 2.0f) : out.beatResp;
    out.ampResp = validFloat(r["amp"]) ? clampFloat(r["amp"].as<float>(), 0.0f, 2.0f) : out.ampResp;
  }
  JsonVariantConst an = src["animation"];
  if (!an.isNull()) {
    out.movement = validFloat(an["movement"]) ? clampFloat(an["movement"].as<float>(), 0.0f, 1.0f) : out.movement;
    out.pulse = validFloat(an["pulse"]) ? clampFloat(an["pulse"].as<float>(), 0.0f, 1.0f) : out.pulse;
    out.sparkle = validFloat(an["sparkle"]) ? clampFloat(an["sparkle"].as<float>(), 0.0f, 1.0f) : out.sparkle;
    out.smoothing = validFloat(an["smoothing"]) ? clampFloat(an["smoothing"].as<float>(), 0.0f, 1.0f) : out.smoothing;
  }
  return true;
}

void ThemeEngine::encode(const ThemeDef& t, JsonObject obj) {
  obj["id"] = t.id;
  obj["name"] = t.name;
  obj["builtin"] = t.builtin;
  obj["brightness"] = t.brightnessPct;
  obj["saturation"] = t.saturation;
  JsonArray pal = obj["palette"].to<JsonArray>();
  char buf[8];
  for (int i = 0; i < t.paletteCount; ++i) pal.add(hexColor(t.palette[i], buf));
  JsonObject r = obj["response"].to<JsonObject>();
  r["bass"] = t.bassResp;
  r["mid"] = t.midResp;
  r["treble"] = t.trebleResp;
  r["beat"] = t.beatResp;
  r["amp"] = t.ampResp;
  JsonObject an = obj["animation"].to<JsonObject>();
  an["movement"] = t.movement;
  an["pulse"] = t.pulse;
  an["sparkle"] = t.sparkle;
  an["smoothing"] = t.smoothing;
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
  f.midResponse = clampFloat(a.mid * def.midResp, 0.0f, 1.0f);
  f.trebleResponse = clampFloat(a.treble * def.trebleResp, 0.0f, 1.0f);
  f.intensity =
      clampFloat(a.amplitude * def.ampResp + beatK * def.beatResp * 0.25f, 0.0f,
                 1.0f);
  f.saturation =
      clampFloat(def.saturation / 255.0f + beatK * def.beatResp * 0.10f, 0.0f,
                 1.0f);
  f.brightness = (def.brightnessPct / 100.0f) * (0.55f + 0.45f * f.intensity);
  f.movement = clampFloat(def.movement * (0.35f + 0.65f * f.bassResponse), 0.0f,
                          1.0f);
  f.pulse = clampFloat(def.pulse * (0.25f + beatK * 0.75f), 0.0f, 1.0f);
  f.sparkle = clampFloat(def.sparkle * f.trebleResponse, 0.0f, 1.0f);
  f.colourShift =
      clampFloat(def.midResp * clampFloat(a.mid, 0.0f, 1.0f) * 0.10f +
                     beatK * def.beatResp * 0.06f,
                 0.0f, 1.0f);
  f.beatResponse = clampFloat(beatK * def.beatResp, 0.0f, 1.0f);
  f.smoothing = clampFloat(def.smoothing, 0.0f, 1.0f);
  f.palette = (def.paletteCount > 0) ? def.palette : nullptr;
  f.paletteCount = def.paletteCount;
  f.themeId = def.id;
  f.themeName = def.name;
  return f;
}

// AUTO: one theme that adapts its own parameters (hysteresis + min duration).
ThemeFrame ThemeEngine::processAuto(const AudioFrame& a, uint32_t nowMs) {
  const float attack = 0.25f;
  const float release = 0.12f;
  const float in = clampFloat(a.amplitude, 0.0f, 1.0f);
  _autoEnergy += (in - _autoEnergy) *
                 (in > _autoEnergy ? attack : release);
  const float e = _autoEnergy;

  // stage boundaries with hysteresis; require the band to hold >= 4 s
  static const float kLo[] = {0.20f, 0.34f, 0.50f, 0.66f};
  static const float kHi[] = {0.30f, 0.44f, 0.60f, 0.76f};
  int next = _autoStage;
  if (nowMs - _autoStageSince < 3000 && e > 0.001f) {
    // still holding a band
  } else if (_autoStage == AUTO_EDM && e < kLo[3]) {
    next = AUTO_AMBIENT;  // hard drop -> straight down (min duration applied)
  } else if (e < kLo[_autoStage]) {
    next = _autoStage > AUTO_AMBIENT ? _autoStage - 1 : AUTO_AMBIENT;
  } else if (e > kHi[_autoStage]) {
    next = _autoStage < AUTO_EDM ? _autoStage + 1 : AUTO_EDM;
  }
  if (next != _autoStage && nowMs - _autoStageSince >= 3000) {
    _autoStage = next;
    _autoStageSince = nowMs;
  }

  ThemeFrame f;
  const Rgb* pal = nullptr;
  int palCount = 0;
  uint8_t bright = 80, sat = 220;
  f.movement = 0.35f;
  f.pulse = 0.4f;
  f.sparkle = 0.4f;
  f.smoothing = 0.4f;
  const char* stageName = "Auto";

  switch (_autoStage) {
    case AUTO_AMBIENT:
      pal = kAutoAmbientPal; palCount = 3; bright = 45; sat = 170;
      f.movement = 0.12f; f.pulse = 0.10f; f.sparkle = 0.05f; f.smoothing = 0.75f;
      stageName = "Ambient";
      break;
    case AUTO_CHILL:
      pal = kAutoChillPal; palCount = 3; bright = 62; sat = 200;
      f.movement = 0.22f; f.pulse = 0.22f; f.sparkle = 0.12f; f.smoothing = 0.6f;
      stageName = "Chill";
      break;
    case AUTO_SPECTRUM:
      pal = kAutoSpectrumPal; palCount = 5; bright = 82; sat = 230;
      f.movement = 0.38f; f.pulse = 0.45f; f.sparkle = 0.45f; f.smoothing = 0.35f;
      stageName = "Spectrum";
      break;
    case AUTO_PARTY:
      pal = kAutoPartyPal; palCount = 4; bright = 95; sat = 255;
      f.movement = 0.6f; f.pulse = 0.85f; f.sparkle = 0.65f; f.smoothing = 0.22f;
      stageName = "Party";
      break;
    case AUTO_EDM:
      pal = kAutoEdmPal; palCount = 3; bright = 100; sat = 255;
      f.movement = 0.9f; f.pulse = 1.0f; f.sparkle = 0.9f; f.smoothing = 0.12f;
      stageName = "EDM";
      break;
  }

  const float beatK = a.beat ? 1.0f : 0.0f;
  f.bassResponse = clampFloat(a.bass, 0.0f, 1.0f);
  f.midResponse = clampFloat(a.mid, 0.0f, 1.0f);
  f.trebleResponse = clampFloat(a.treble, 0.0f, 1.0f);
  f.intensity = e;
  f.saturation = sat / 255.0f + beatK * 0.12f;
  if (f.saturation > 1.0f) f.saturation = 1.0f;
  f.brightness = (bright / 100.0f) * (0.55f + 0.45f * e);
  f.pulse = clampFloat(f.pulse * (0.25f + beatK * 0.75f), 0.0f, 1.0f);
  f.sparkle = clampFloat(f.sparkle * f.trebleResponse, 0.0f, 1.0f);
  f.colourShift = beatK * 0.05f;
  f.beatResponse = clampFloat(beatK, 0.0f, 1.0f);
  f.palette = pal;
  f.paletteCount = palCount;
  f.themeId = Themes::kAutoThemeId;
  f.themeName = stageName;
  return f;
}

ThemeFrame ThemeEngine::process(const AudioFrame& a, uint32_t nowMs) {
  if (!_mutex) return identityFrame();
  bool autoMode = false;
  ThemeDef def;
  bool have = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    const char* id = activeId();
    if (strncmp(id, Themes::kAutoThemeId, sizeof(Themes::kAutoThemeId)) == 0) {
      autoMode = true;
    } else {
      const ThemeDef* p = resolve(id);
      if (p) {
        def = *p;  // copy under lock; LED task never reads a torn write
        have = true;
      }
    }
    xSemaphoreGive(_mutex);
  }
  if (autoMode) return processAuto(a, nowMs);
  return have ? processTheme(def, a, nowMs) : identityFrame();
}

ThemeFrame ThemeEngine::processStrip(int strip, const AudioFrame& a,
                                     uint32_t nowMs) {
  if (!_mutex || strip < 0 || strip >= kMaxStrips) return identityFrame();
  bool autoMode = false;
  ThemeDef def;
  bool have = false;
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    const char* id = _active[strip][0] ? _active[strip] : activeId();
    if (strncmp(id, Themes::kAutoThemeId, sizeof(Themes::kAutoThemeId)) == 0) {
      autoMode = true;
    } else {
      const ThemeDef* p = resolve(id);
      if (p) {
        def = *p;
        have = true;
      }
    }
    xSemaphoreGive(_mutex);
  }
  if (autoMode) return processAuto(a, nowMs);
  return have ? processTheme(def, a, nowMs) : identityFrame();
}