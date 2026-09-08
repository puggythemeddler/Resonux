#include "config/ConfigStore.h"
#include "config/ConfigDefaults.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char kConfigPath[] = "/config.json";
static const int  kConfigVersion = 1;

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

bool validFloat(float v) {
  return !isnan(v) && !isinf(v);
}

void serializeBands(JsonArray bands, const AudioAnalyzerConfig& a) {
  for (int b = 0; b < a.bandCount; ++b) {
    JsonObject o = bands.add<JsonObject>();
    o["lo"] = a.bands[b].loHz;
    o["hi"] = a.bands[b].hiHz;
  }
}

void deserializeBands(JsonArray bands, AudioAnalyzerConfig& a) {
  int n = clampInt((int)bands.size(), 1, kMaxBands);
  a.bandCount = n;
  for (int b = 0; b < n; ++b) {
    JsonObject o = bands[b];
    a.bands[b].loHz = o["lo"] | a.bands[b].loHz;
    a.bands[b].hiHz = o["hi"] | a.bands[b].hiHz;
  }
}

void serializeStrip(JsonObject s, const StripConfig& c) {
  s["name"] = c.name;
  s["themeId"] = c.themeId;
  s["driverType"] = c.driverType;
  s["chipset"] = c.chipset;
  s["colorOrder"] = c.colorOrder;
  s["dataPin"] = c.dataPin;
  s["clockPin"] = c.clockPin;
  s["ledCount"] = c.ledCount;
  s["reverse"] = c.reverse;
  s["zoneCount"] = c.zoneCount;
  s["commonAnode"] = c.commonAnode;
  s["pwmFreqHz"] = c.pwmFreqHz;
  s["effectId"] = c.effectId;
  s["maxBrightness"] = c.maxBrightness;
  s["minBrightness"] = c.minBrightness;
  s["maxCurrentA"] = c.maxCurrentA;
  s["maxVolts"] = c.maxVolts;
  s["palette"] = c.palette;
  s["startHue"] = c.startHue;
  s["hueSpeed"] = c.hueSpeed;
  s["sensitivity"] = c.sensitivity;
  s["decay"] = c.decay;
  s["targetFps"] = c.targetFps;
  JsonArray zones = s["zones"].to<JsonArray>();
  for (int z = 0; z < c.zoneCount; ++z) {
    JsonObject zo = zones.add<JsonObject>();
    JsonArray pins = zo["pins"].to<JsonArray>();
    for (int j = 0; j < 3; ++j) pins.add(c.zones[z].pins[j]);
    zo["nPins"] = c.zones[z].nPins;
    zo["pos"] = c.zones[z].pos01;
    zo["band"] = c.zones[z].band;
  }
}

void deserializeStrip(JsonObject s, StripConfig& c) {
  strncpy(c.name, s["name"] | c.name, sizeof(c.name) - 1);
  strncpy(c.themeId, s["themeId"] | c.themeId, sizeof(c.themeId) - 1);
  c.driverType = clampInt(s["driverType"] | c.driverType, 0, DRIVER_TYPE_COUNT - 1);
  c.chipset = clampInt(s["chipset"] | c.chipset, 0, CHIP_COUNT - 1);
  c.colorOrder = clampInt(s["colorOrder"] | c.colorOrder, 0, ORDER_COUNT - 1);
  c.dataPin = s["dataPin"] | c.dataPin;
  c.clockPin = s["clockPin"] | c.clockPin;
  c.ledCount = clampInt(s["ledCount"] | c.ledCount, 1, 1000);
  c.reverse = s["reverse"] | c.reverse;
  c.zoneCount = clampInt(s["zoneCount"] | c.zoneCount, 1, kMaxStripZones);
  c.commonAnode = s["commonAnode"] | c.commonAnode;
  c.pwmFreqHz = validFloat(s["pwmFreqHz"]) ? clampFloat(s["pwmFreqHz"].as<float>(), 50.0f, 100000.0f) : c.pwmFreqHz;
  c.effectId = clampInt(s["effectId"] | c.effectId, 0, EFFECT_COUNT - 1);
  c.maxBrightness = clampInt(s["maxBrightness"] | c.maxBrightness, 0, 255);
  c.minBrightness = clampInt(s["minBrightness"] | c.minBrightness, 0, 255);
  c.maxCurrentA = validFloat(s["maxCurrentA"]) ? clampFloat(s["maxCurrentA"].as<float>(), 0.1f, 100.0f) : c.maxCurrentA;
  c.maxVolts = validFloat(s["maxVolts"]) ? clampFloat(s["maxVolts"].as<float>(), 1.0f, 60.0f) : c.maxVolts;
  c.palette = clampInt(s["palette"] | c.palette, 0, PALETTE_COUNT - 1);
  c.startHue = clampInt(s["startHue"] | c.startHue, 0, 360);
  c.hueSpeed = validFloat(s["hueSpeed"]) ? clampFloat(s["hueSpeed"].as<float>(), 0.0f, 360.0f) : c.hueSpeed;
  c.sensitivity = validFloat(s["sensitivity"]) ? clampFloat(s["sensitivity"].as<float>(), 0.0f, 10.0f) : c.sensitivity;
  c.decay = validFloat(s["decay"]) ? clampFloat(s["decay"].as<float>(), 0.0f, 1.0f) : c.decay;
  c.targetFps = clampInt(s["targetFps"] | c.targetFps, 1, 120);
  if (s["zones"].is<JsonArray>()) {
    JsonArray zones = s["zones"].as<JsonArray>();
    int zn = clampInt((int)zones.size(), 0, kMaxStripZones);
    for (int z = 0; z < zn; ++z) {
      JsonObject zo = zones[z];
      StripZone& sz = c.zones[z];
      if (zo["pins"].is<JsonArray>()) {
        JsonArray pins = zo["pins"].as<JsonArray>();
        for (int j = 0; j < 3; ++j) sz.pins[j] = pins[j] | -1;
      }
      sz.nPins = clampInt(zo["nPins"] | sz.nPins, 1, 3);
      sz.pos01 = zo["pos"] | sz.pos01;
      sz.band = zo["band"] | sz.band;
    }
  }
}

void serializeDisplay(JsonObject d, const DisplayConfig& c) {
  d["enabled"] = c.enabled;
  d["panel"] = c.panel;
  d["touch"] = c.touch;
  d["orientation"] = c.orientation;
  d["spiSck"] = c.spiSck;
  d["spiMosi"] = c.spiMosi;
  d["spiMiso"] = c.spiMiso;
  d["csPin"] = c.csPin;
  d["dcPin"] = c.dcPin;
  d["rstPin"] = c.rstPin;
  d["blPin"] = c.blPin;
  d["touchSda"] = c.touchSda;
  d["touchScl"] = c.touchScl;
  d["touchIrq"] = c.touchIrq;
  d["touchRst"] = c.touchRst;
  d["wakePin"] = c.wakePin;
  d["logicalW"] = c.logicalW;
  d["logicalH"] = c.logicalH;
  d["backlightPct"] = c.backlightPct;
  d["screenTimeoutS"] = c.screenTimeoutS;
  d["uiFps"] = c.uiFps;
}

void deserializeDisplay(JsonObject d, DisplayConfig& c) {
  c.enabled = d["enabled"] | c.enabled;
  c.panel = clampInt(d["panel"] | c.panel, 0, DISPLAY_PANEL_COUNT - 1);
  c.touch = clampInt(d["touch"] | c.touch, 0, TOUCH_CHIP_COUNT - 1);
  c.orientation = clampInt(d["orientation"] | c.orientation, 0, 1);
  c.spiSck = d["spiSck"] | c.spiSck;
  c.spiMosi = d["spiMosi"] | c.spiMosi;
  c.spiMiso = d["spiMiso"] | c.spiMiso;
  c.csPin = d["csPin"] | c.csPin;
  c.dcPin = d["dcPin"] | c.dcPin;
  c.rstPin = d["rstPin"] | c.rstPin;
  c.blPin = d["blPin"] | c.blPin;
  c.touchSda = d["touchSda"] | c.touchSda;
  c.touchScl = d["touchScl"] | c.touchScl;
  c.touchIrq = d["touchIrq"] | c.touchIrq;
  c.touchRst = d["touchRst"] | c.touchRst;
  c.wakePin = d["wakePin"] | c.wakePin;
  c.logicalW = clampInt(d["logicalW"] | c.logicalW, 160, 1600);
  c.logicalH = clampInt(d["logicalH"] | c.logicalH, 160, 1600);
  c.backlightPct = clampInt(d["backlightPct"] | c.backlightPct, 0, 100);
  c.screenTimeoutS = clampInt(d["screenTimeoutS"] | c.screenTimeoutS, 0, 86400);
  c.uiFps = clampInt(d["uiFps"] | c.uiFps, 5, 120);
}

void serializeSync(JsonObject s, const SyncConfig& c) {
  s["enabled"] = c.enabled;
  s["role"] = c.role;
  s["group"] = c.group;
  s["port"] = c.port;
  s["heartbeatMs"] = c.heartbeatMs;
  s["timeoutMs"] = c.timeoutMs;
}

void deserializeSync(JsonObject s, SyncConfig& c) {
  c.enabled = s["enabled"] | c.enabled;
  c.role = clampInt(s["role"] | c.role, 0, 2);
  strncpy(c.group, s["group"] | c.group, sizeof(c.group) - 1);
  c.port = clampInt(s["port"] | c.port, 1024, 65535);
  c.heartbeatMs = clampInt(s["heartbeatMs"] | c.heartbeatMs, 10, 1000);
  c.timeoutMs = clampInt(s["timeoutMs"] | c.timeoutMs, 100, 10000);
}

}  // namespace

bool ConfigStore::begin() {
  return LittleFS.begin(true);
}

bool ConfigStore::exists() {
  return LittleFS.exists(kConfigPath);
}

bool ConfigStore::load(Config& cfg) {
  configDefaults(cfg);
  if (!exists()) return false;
  File f = LittleFS.open(kConfigPath, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  int ver = doc["version"] | 0;
  if (ver > kConfigVersion) return false;  // config from a future build—do not clobber

  strncpy(cfg.deviceName, doc["deviceName"] | cfg.deviceName,
          sizeof(cfg.deviceName) - 1);
  strncpy(cfg.themeId, doc["themeId"] | cfg.themeId, sizeof(cfg.themeId) - 1);
  cfg.masterBrightness =
      clampInt(doc["masterBrightness"] | cfg.masterBrightness, 0, 255);
  cfg.stripCount = clampInt(doc["stripCount"] | cfg.stripCount, 1, kMaxStrips);

  JsonObject ao = doc["audio"].as<JsonObject>();
  AudioAnalyzerConfig& a = cfg.audio;
  if (!ao.isNull()) {
    a.sampleRate = clampInt(ao["sampleRate"] | a.sampleRate, 8000, 96000);
    a.fftSize = clampInt(ao["fftSize"] | a.fftSize, 256, 2048);
    a.hopSize = clampInt(ao["hopSize"] | a.hopSize, 64, a.fftSize);
    a.gain = validFloat(ao["gain"]) ? clampFloat(ao["gain"].as<float>(), 0.1f, 20.0f) : a.gain;
    a.normMode = clampInt(ao["normMode"] | a.normMode, 0, 1);
    a.dbFloor = validFloat(ao["dbFloor"]) ? ao["dbFloor"].as<float>() : a.dbFloor;
    a.dbCeil = validFloat(ao["dbCeil"]) ? ao["dbCeil"].as<float>() : a.dbCeil;
    a.noiseGate = validFloat(ao["noiseGate"]) ? clampFloat(ao["noiseGate"].as<float>(), 0.0f, 0.9f) : a.noiseGate;
    a.smooth = ao["smooth"] | a.smooth;
    a.attack = validFloat(ao["attack"]) ? clampFloat(ao["attack"].as<float>(), 0.01f, 0.99f) : a.attack;
    a.release = validFloat(ao["release"]) ? clampFloat(ao["release"].as<float>(), 0.005f, 0.99f) : a.release;
    a.beatDetect = ao["beatDetect"] | a.beatDetect;
    a.beatSens = validFloat(ao["beatSens"]) ? clampFloat(ao["beatSens"].as<float>(), 0.1f, 5.0f) : a.beatSens;
    a.beatMinGapMs = clampInt(ao["beatMinGapMs"] | a.beatMinGapMs, 50, 2000);
    a.beatLowBands = clampInt(ao["beatLowBands"] | a.beatLowBands, 1, kMaxBands);
    a.ampGain = validFloat(ao["ampGain"]) ? clampFloat(ao["ampGain"].as<float>(), 0.1f, 10.0f) : a.ampGain;
    a.ampDbFloor = validFloat(ao["ampDbFloor"]) ? ao["ampDbFloor"].as<float>() : a.ampDbFloor;
    a.ampDbCeil = validFloat(ao["ampDbCeil"]) ? ao["ampDbCeil"].as<float>() : a.ampDbCeil;
    if (ao["bands"].is<JsonArray>()) {
      deserializeBands(ao["bands"].as<JsonArray>(), a);
    }
    if (ao["groups"].is<JsonArray>()) {
      JsonArray groups = ao["groups"].as<JsonArray>();
      for (int g = 0; g < 5; ++g) {
        JsonArray r = groups[g];
        if (!r.isNull()) {
          a.groupRanges[g][0] = clampInt(r[0] | a.groupRanges[g][0], 0, kMaxBands - 1);
          a.groupRanges[g][1] = clampInt(r[1] | a.groupRanges[g][1], a.groupRanges[g][0], kMaxBands - 1);
        }
      }
    }
  }

  cfg.micSck = doc["micSck"] | cfg.micSck;
  cfg.micWs = doc["micWs"] | cfg.micWs;
  cfg.micData = doc["micData"] | cfg.micData;

  if (doc["strips"].is<JsonArray>()) {
    JsonArray strips = doc["strips"].as<JsonArray>();
    for (int i = 0; i < cfg.stripCount; ++i) {
      if (i >= kMaxStrips) break;
      deserializeStrip(strips[i].as<JsonObject>(), cfg.strips[i]);
    }
  }

  JsonObject nw = doc["net"].as<JsonObject>();
  if (!nw.isNull()) {
    cfg.net.enabled = nw["enabled"] | cfg.net.enabled;
    cfg.net.mode = clampInt(nw["mode"] | cfg.net.mode, 0, 2);
    strncpy(cfg.net.apSsid, nw["apSsid"] | cfg.net.apSsid,
            sizeof(cfg.net.apSsid) - 1);
    strncpy(cfg.net.apPassword, nw["apPassword"] | cfg.net.apPassword,
            sizeof(cfg.net.apPassword) - 1);
    strncpy(cfg.net.staSsid, nw["staSsid"] | cfg.net.staSsid,
            sizeof(cfg.net.staSsid) - 1);
    strncpy(cfg.net.staPassword, nw["staPassword"] | cfg.net.staPassword,
            sizeof(cfg.net.staPassword) - 1);
  }

  JsonObject an = doc["artnet"].as<JsonObject>();
  if (!an.isNull()) {
    cfg.artnet.enabled = an["enabled"] | cfg.artnet.enabled;
    strncpy(cfg.artnet.ssid, an["ssid"] | cfg.artnet.ssid,
            sizeof(cfg.artnet.ssid) - 1);
    strncpy(cfg.artnet.password, an["password"] | cfg.artnet.password,
            sizeof(cfg.artnet.password) - 1);
    cfg.artnet.useDhcp = an["useDhcp"] | cfg.artnet.useDhcp;
    if (an["staticIp"].is<JsonArray>()) {
      JsonArray a = an["staticIp"].as<JsonArray>();
      for (int i = 0; i < 4; ++i) cfg.artnet.staticIp[i] = a[i] | 0;
    }
    if (an["staticMask"].is<JsonArray>()) {
      JsonArray a = an["staticMask"].as<JsonArray>();
      for (int i = 0; i < 4; ++i) cfg.artnet.staticMask[i] = a[i] | 0;
    }
    if (an["staticGw"].is<JsonArray>()) {
      JsonArray a = an["staticGw"].as<JsonArray>();
      for (int i = 0; i < 4; ++i) cfg.artnet.staticGw[i] = a[i] | 0;
    }
    cfg.artnet.universe = an["universe"] | cfg.artnet.universe;
    cfg.artnet.audioReactive = an["audioReactive"] | cfg.artnet.audioReactive;
    cfg.artnet.panSpeed = validFloat(an["panSpeed"]) ? clampFloat(an["panSpeed"].as<float>(), 0.0f, 1.0f) : cfg.artnet.panSpeed;
    cfg.artnet.tiltSpeed = validFloat(an["tiltSpeed"]) ? clampFloat(an["tiltSpeed"].as<float>(), 0.0f, 1.0f) : cfg.artnet.tiltSpeed;
    cfg.artnet.colorSensitivity = validFloat(an["colorSensitivity"]) ? clampFloat(an["colorSensitivity"].as<float>(), 0.0f, 2.0f) : cfg.artnet.colorSensitivity;
  }

  JsonObject disp = doc["display"].as<JsonObject>();
  if (!disp.isNull()) {
    deserializeDisplay(disp, cfg.display);
  }

  JsonObject sy = doc["sync"].as<JsonObject>();
  if (!sy.isNull()) {
    deserializeSync(sy, cfg.sync);
  }

  if (doc["fixtures"].is<JsonArray>()) {
    JsonArray fixes = doc["fixtures"].as<JsonArray>();
    cfg.fixtureCount =
        clampInt((int)fixes.size(), 0, kMaxFixtures);
    for (int i = 0; i < cfg.fixtureCount; ++i) {
      JsonObject fx = fixes[i].as<JsonObject>();
      cfg.fixtures[i].profileId =
          clampInt(fx["profileId"] | 0, 0, FIXTURE_COUNT - 1);
      cfg.fixtures[i].dmxAddress =
          clampInt(fx["dmxAddress"] | 1, 1, kDmxChannels);
      cfg.fixtures[i].count = clampInt(fx["count"] | 0, 0, 16);
    }
  }
  return true;
}

void buildDoc(const Config& cfg, JsonDocument& doc) {
  doc["version"] = kConfigVersion;
  doc["deviceName"] = cfg.deviceName;
  doc["themeId"] = cfg.themeId;
  doc["masterBrightness"] = cfg.masterBrightness;
  doc["stripCount"] = cfg.stripCount;
  doc["micSck"] = cfg.micSck;
  doc["micWs"] = cfg.micWs;
  doc["micData"] = cfg.micData;

  JsonObject ao = doc["audio"].to<JsonObject>();
  ao["sampleRate"] = cfg.audio.sampleRate;
  ao["fftSize"] = cfg.audio.fftSize;
  ao["hopSize"] = cfg.audio.hopSize;
  ao["gain"] = cfg.audio.gain;
  ao["normMode"] = cfg.audio.normMode;
  ao["dbFloor"] = cfg.audio.dbFloor;
  ao["dbCeil"] = cfg.audio.dbCeil;
  ao["noiseGate"] = cfg.audio.noiseGate;
  ao["smooth"] = cfg.audio.smooth;
  ao["attack"] = cfg.audio.attack;
  ao["release"] = cfg.audio.release;
  ao["beatDetect"] = cfg.audio.beatDetect;
  ao["beatSens"] = cfg.audio.beatSens;
  ao["beatMinGapMs"] = cfg.audio.beatMinGapMs;
  ao["beatLowBands"] = cfg.audio.beatLowBands;
  ao["ampGain"] = cfg.audio.ampGain;
  ao["ampDbFloor"] = cfg.audio.ampDbFloor;
  ao["ampDbCeil"] = cfg.audio.ampDbCeil;
  serializeBands(ao["bands"].to<JsonArray>(), cfg.audio);
  JsonArray groups = ao["groups"].to<JsonArray>();
  for (int g = 0; g < 5; ++g) {
    JsonArray r = groups.add<JsonArray>();
    r.add(cfg.audio.groupRanges[g][0]);
    r.add(cfg.audio.groupRanges[g][1]);
  }

  JsonArray strips = doc["strips"].to<JsonArray>();
  for (int i = 0; i < cfg.stripCount; ++i) {
    JsonObject s = strips.add<JsonObject>();
    serializeStrip(s, cfg.strips[i]);
  }

  JsonObject nw = doc["net"].to<JsonObject>();
  nw["enabled"] = cfg.net.enabled;
  nw["mode"] = cfg.net.mode;
  nw["apSsid"] = cfg.net.apSsid;
  nw["apPassword"] = cfg.net.apPassword;
  nw["staSsid"] = cfg.net.staSsid;
  nw["staPassword"] = cfg.net.staPassword;

  JsonObject an = doc["artnet"].to<JsonObject>();
  an["enabled"] = cfg.artnet.enabled;
  an["ssid"] = cfg.artnet.ssid;
  an["password"] = cfg.artnet.password;
  an["useDhcp"] = cfg.artnet.useDhcp;
  JsonArray sip = an["staticIp"].to<JsonArray>();
  for (int i = 0; i < 4; ++i) sip.add(cfg.artnet.staticIp[i]);
  JsonArray smask = an["staticMask"].to<JsonArray>();
  for (int i = 0; i < 4; ++i) smask.add(cfg.artnet.staticMask[i]);
  JsonArray sgw = an["staticGw"].to<JsonArray>();
  for (int i = 0; i < 4; ++i) sgw.add(cfg.artnet.staticGw[i]);
  an["universe"] = cfg.artnet.universe;
  an["audioReactive"] = cfg.artnet.audioReactive;
  an["panSpeed"] = cfg.artnet.panSpeed;
  an["tiltSpeed"] = cfg.artnet.tiltSpeed;
  an["colorSensitivity"] = cfg.artnet.colorSensitivity;

  JsonObject disp = doc["display"].to<JsonObject>();
  serializeDisplay(disp, cfg.display);

  JsonObject sy = doc["sync"].to<JsonObject>();
  serializeSync(sy, cfg.sync);

  JsonArray fixes = doc["fixtures"].to<JsonArray>();
  for (int i = 0; i < cfg.fixtureCount; ++i) {
    JsonObject fx = fixes.add<JsonObject>();
    fx["profileId"] = cfg.fixtures[i].profileId;
    fx["dmxAddress"] = cfg.fixtures[i].dmxAddress;
    fx["count"] = cfg.fixtures[i].count;
  }
}

bool ConfigStore::save(const Config& cfg) {
  JsonDocument doc;
  buildDoc(cfg, doc);

  File f = LittleFS.open(kConfigPath, "w");
  if (!f) return false;
  size_t w = serializeJson(doc, f);
  f.close();
  return w > 0;
}

String ConfigStore::dumpString(const Config& cfg) {
  JsonDocument doc;
  buildDoc(cfg, doc);
  String out;
  serializeJson(doc, out);
  return out;
}

void ConfigStore::printToSerial(const Config& cfg) {
  JsonDocument doc;
  buildDoc(cfg, doc);
  serializeJson(doc, Serial);
  Serial.println();
}