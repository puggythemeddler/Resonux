#include "web/WebUi.h"
#include "audio/AudioFrame.h"
#include "audio/SourceKind.h"
#include "config/ConfigStore.h"
#include "device/DeviceManager.h"
#include "device/DeviceProfile.h"
#include "network/WifiManager.h"
#include "runtime/App.h"
#include "sync/SyncNode.h"
#include "theme/ThemeEngine.h"
#include "util/Log.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>

bool WebUi::begin(App& app) {
  _app = &app;
  _startMs = millis();

  if (!LittleFS.begin(true)) {
    logBoot("web", "LittleFS FAILED");
    return false;
  }

  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/api/status", HTTP_GET, [this]() { sendStatus(); });
  _server.on("/api/frame", HTTP_GET, [this]() { sendFrame(); });
  _server.on("/api/config", HTTP_GET, [this]() { sendConfig(); });
  _server.on("/api/config", HTTP_PUT, [this]() { handleConfigPut(); });
  _server.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
  _server.on("/api/state", HTTP_GET, [this]() { sendState(); });
  _server.on("/api/state/brightness", HTTP_POST, [this]() { handleStateBrightness(); });
  _server.on("/api/state/effect", HTTP_POST, [this]() { handleStateEffect(); });
  _server.on("/api/state/sensitivity", HTTP_POST, [this]() { handleStateSensitivity(); });
  _server.on("/api/state/backlight", HTTP_POST, [this]() { handleStateBacklight(); });
  _server.on("/api/state/timeout", HTTP_POST, [this]() { handleStateTimeout(); });
  _server.on("/api/system/status", HTTP_GET, [this]() { sendSystemStatus(); });
  _server.on("/api/system/restart", HTTP_POST, [this]() { handleSystemRestart(); });
  _server.on("/api/system/power-off", HTTP_POST, [this]() { handleSystemPowerOff(); });
  _server.on("/api/themes", HTTP_GET, [this]() { sendThemes(); });
  _server.on("/api/themes", HTTP_PUT, [this]() { handleThemesPut(); });
  _server.on("/api/themes", HTTP_DELETE, [this]() { handleThemesDelete(); });
  _server.on("/api/themes/select", HTTP_POST, [this]() { handleThemesSelect(); });
  _server.on("/api/themes/reset", HTTP_POST, [this]() { handleThemesReset(); });
  _server.on("/api/devices", HTTP_GET, [this]() { sendDevices(); });
  _server.on("/api/devices/scan", HTTP_POST, [this]() { handleDevicesScan(); });
  _server.on("/api/device", HTTP_GET, [this]() { handleDevice(); });
  _server.on("/api/device", HTTP_POST, [this]() { handleDevice(); });
  _server.on("/api/device", HTTP_DELETE, [this]() { handleDevice(); });
  _server.on("/api/audio/sources", HTTP_GET, [this]() { sendAudioSources(); });
  _server.on("/api/audio/source", HTTP_POST, [this]() { handleAudioSource(); });
  _server.on(
      "/api/ota", HTTP_POST,
      [this]() {
        _server.sendHeader("Connection", "close");
        _server.send(Update.hasError() ? 500 : 200, "text/plain",
                     Update.hasError() ? "FAIL" : "OK");
        if (!Update.hasError()) {
          logBoot("ota", "update OK, rebooting");
          delay(500);
          ESP.restart();
        }
      },
      [this]() {
        HTTPUpload& up = _server.upload();
        if (up.status == UPLOAD_FILE_START) {
          logBoot("ota", "start %s (%u bytes)", up.filename.c_str(),
                  up.totalSize);
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
          }
        } else if (up.status == UPLOAD_FILE_WRITE) {
          if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            Update.printError(Serial);
          }
        } else if (up.status == UPLOAD_FILE_END) {
          if (!Update.end(true)) Update.printError(Serial);
        }
      });
  _server.onNotFound([this]() { handleStatic(_server.uri()); });
  _server.begin();

  ArduinoOTA.setHostname("resonux");
  ArduinoOTA.begin();

  xTaskCreatePinnedToCore(WebUi::taskEntry, "web", 6144, this, 15, &_task, 0);

  logBoot("web", "server ready http://%s/",
          WifiManager::instance().ip().toString().c_str());
  return true;
}

void WebUi::stop() {
  _running = false;
  if (_task) {
    vTaskDelay(pdMS_TO_TICKS(50));
    _task = nullptr;
  }
  _server.stop();
  _server.close();
  ArduinoOTA.end();
}

void WebUi::taskEntry(void* arg) {
  WebUi* ui = (WebUi*)arg;
  ui->_running = true;
  while (ui->_running) {
    ui->loop();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void WebUi::loop() {
  _server.handleClient();
  ArduinoOTA.handle();
}

void WebUi::handleRoot() {
  File f = LittleFS.open("/web/index.html", "r");
  if (!f) {
    _server.send(503, "text/html",
                 "Dashboard not embedded in flash. Build /web and upload with "
                 "`pio run -t uploadfs`.");
    return;
  }
  _server.streamFile(f, "text/html");
  f.close();
}

String WebUi::mimeFor(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".ico")) return "image/x-icon";
  if (path.endsWith(".woff2")) return "font/woff2";
  return "text/plain";
}

void WebUi::handleStatic(const String& uri) {
  String path = uri;
  if (path == "/") path = "/web/index.html";
  if (!path.startsWith("/web/")) {
    _server.send(404, "text/plain", "Not found");
    return;
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    _server.send(404, "text/plain", "Not found");
    return;
  }
  _server.streamFile(f, mimeFor(path));
  f.close();
}

void WebUi::sendStatus() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["device"] = _app->config().deviceName;
  doc["uptimeMs"] = millis() - _startMs;
  doc["heap"] = (uint32_t)ESP.getFreeHeap();
  doc["fps"] = _app->audioFps();
  doc["stripCount"] = _app->config().stripCount;

  WifiManager& wifi = WifiManager::instance();
  JsonObject w = doc["wifi"].to<JsonObject>();
  w["mode"] = wifi.modeName();
  w["ip"] = wifi.ip().toString();
  w["connected"] = wifi.connected();

  JsonObject sy = doc["sync"].to<JsonObject>();
  sy["enabled"] = _app->config().sync.enabled;
  sy["role"] = _app->sync() ? _app->sync()->roleName() : "off";
  if (_app->sync() && _app->sync()->enabled()) {
    sy["seq"] = _app->sync()->seq();
    sy["offsetMs"] = _app->sync()->clockOffsetMs();
    sy["group"] = _app->config().sync.group;
    sy["port"] = _app->config().sync.port;
    if (_app->sync()->role() == SYNC_SLAVE) {
      sy["masterAlive"] = _app->sync()->masterAlive();
    }
  }

  JsonObject cn = doc["artnet"].to<JsonObject>();
  cn["enabled"] = _app->config().artnet.enabled;
  cn["fixtures"] = _app->config().fixtureCount;
  if (_app->artnet()) {
    cn["status"] = _app->artnet()->statusString();
  } else {
    cn["status"] = "off";
  }

  JsonObject sg = doc["system"].to<JsonObject>();
  sg["state"] = _app->systemStateName();
  sg["action"] = _app->systemMode().actionName();

  JsonObject dp = doc["display"].to<JsonObject>();
  dp["enabled"] = _app->config().display.enabled;
  dp["backlightPct"] = DisplayManager::instance().backlightPct();
  dp["timeoutS"] = _app->config().display.screenTimeoutS;
  dp["awake"] = DisplayManager::instance().isAwake();
  dp["wakePin"] = _app->config().display.wakePin;

  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::sendFrame() {
  AudioFrame f;
  bool got = _app->takeFrame(f);
  JsonDocument doc;
  doc["ok"] = got;
  doc["fps"] = _app->audioFps();
  if (got) {
    doc["amp"] = f.amplitude;
    doc["bass"] = f.bass;
    doc["lowMid"] = f.lowMid;
    doc["mid"] = f.mid;
    doc["highMid"] = f.highMid;
    doc["treble"] = f.treble;
    doc["beat"] = f.beat;
    doc["beatStrength"] = f.beatStrength;
    doc["bandCount"] = f.bandCount;
    JsonArray bands = doc["bands"].to<JsonArray>();
    for (int i = 0; i < f.bandCount; ++i) bands.add(f.bands[i]);
    JsonArray peaks = doc["peaks"].to<JsonArray>();
    for (int i = 0; i < f.bandCount; ++i) peaks.add(f.peaks[i]);
  }
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::sendConfig() {
  _server.send(200, "application/json", ConfigStore::dumpString(_app->config()));
}

void WebUi::handleConfigPut() {
  if (_server.hasArg("plain")) {
    String body = _server.arg("plain");
    File f = LittleFS.open("/config.json", "w");
    if (f) {
      f.print(body);
      f.close();
      logBoot("web", "config saved via PUT (%u bytes), rebooting",
              (unsigned)body.length());
      _server.send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
      delay(500);
      ESP.restart();
      return;
    }
  }
  _server.send(400, "application/json", "{\"ok\":false}");
}

void WebUi::handleReboot() {
  _server.send(200, "application/json", "{\"ok\":true,\"action\":\"restart\"}");
  logBoot("sys", "restart requested from dashboard (legacy /api/reboot)");
  _app->requestRestart();
}

static bool readIntArg(JsonDocument& doc, const String& plain, int& value,
                       int lo, int hi) {
  if (deserializeJson(doc, plain) != DeserializationError::Ok) return false;
  if (!doc["value"].is<int>()) return false;
  value = doc["value"].as<int>();
  return value >= lo && value <= hi;
}

void WebUi::handleStateSensitivity() {
  JsonDocument doc;
  if (deserializeJson(doc, _server.arg("plain")) != DeserializationError::Ok ||
      !doc["value"].is<float>()) {
    _server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  bool ok = _app->setSensitivity(doc["value"].as<float>());
  _server.send(ok ? 200 : 400, "application/json",
               ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebUi::handleStateBacklight() {
  JsonDocument doc;
  int v = -1;
  if (!readIntArg(doc, _server.arg("plain"), v, 0, 100)) {
    _server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  bool ok = _app->setDisplayBacklightPct(v);
  _server.send(ok ? 200 : 400, "application/json",
               ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebUi::handleStateTimeout() {
  JsonDocument doc;
  int v = -1;
  if (!readIntArg(doc, _server.arg("plain"), v, 0, 86400)) {
    _server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  bool ok = _app->setDisplayTimeout(v);
  _server.send(ok ? 200 : 400, "application/json",
               ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebUi::sendSystemStatus() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["state"] = _app->systemStateName();
  doc["action"] = _app->systemMode().actionName();
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::handleSystemRestart() {
  _server.send(200, "application/json",
               "{\"ok\":true,\"action\":\"restart\"}");
  logBoot("sys", "restart requested from dashboard");
  _app->requestRestart();
}

void WebUi::handleSystemPowerOff() {
  _server.send(200, "application/json",
               "{\"ok\":true,\"action\":\"power_off\"}");
  logBoot("sys", "power-off requested from dashboard");
  _app->requestPowerOff();
}

// ---------------------------------------------------------------- themes
void WebUi::sendState() {
  const Config& c = _app->config();
  ThemeEngine& te = ThemeEngine::instance();
  JsonDocument doc;
  JsonObject th = doc["theme"].to<JsonObject>();
  th["global"] = te.activeId();
  JsonArray strips = th["strips"].to<JsonArray>();
  for (int i = 0; i < c.stripCount; ++i) strips.add(te.activeStripId(i));
  JsonArray effects = doc["effect"].to<JsonArray>();
  for (int i = 0; i < c.stripCount; ++i) effects.add(c.strips[i].effectId);
  doc["brightness"] = c.masterBrightness;
  doc["stripCount"] = c.stripCount;
  doc["themeCount"] = te.count();
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::handleStateBrightness() {
  if (!_server.hasArg("plain")) {
    _server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, _server.arg("plain"))) {
    _server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  int v = doc["value"] | -1;
  if (v < 0 || v > 255) {
    _server.send(400, "application/json", "{\"error\":\"value 0..255\"}");
    return;
  }
  _app->setMasterBrightness((uint8_t)v);
  _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleStateEffect() {
  if (!_server.hasArg("plain")) {
    _server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, _server.arg("plain"))) {
    _server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  int strip = doc["strip"] | -1;
  int effectId = doc["effectId"] | -1;
  if (strip < 0 || effectId < 0 || !_app->setStripEffect(strip, effectId)) {
    _server.send(400, "application/json", "{\"error\":\"invalid strip/effect\"}");
    return;
  }
  _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::sendThemes() {
  ThemeEngine& te = ThemeEngine::instance();
  JsonDocument doc;
  JsonArray arr = doc["themes"].to<JsonArray>();
  for (int i = 0; i < te.count(); ++i) {
    JsonObject o = arr.add<JsonObject>();
    ThemeEngine::encode(*te.get(i), o);
  }
  doc["active"] = te.activeId();
  JsonArray strips = doc["strips"].to<JsonArray>();
  const Config& c = _app->config();
  for (int i = 0; i < c.stripCount; ++i) strips.add(te.activeStripId(i));
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::handleThemesPut() {
  if (!_server.hasArg("plain")) {
    _server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, _server.arg("plain"))) {
    _server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  ThemeDef t;
  char err[160] = "";
  if (!ThemeEngine::decode(doc.as<JsonVariantConst>(), t, err, sizeof(err))) {
    _server.send(400, "application/json", String("{\"error\":\"") + err + "\"}");
    return;
  }
  if (!ThemeEngine::instance().update(t, true, err, sizeof(err))) {
    _server.send(400, "application/json", String("{\"error\":\"") + err + "\"}");
    return;
  }
  _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleThemesDelete() {
  String id = _server.arg("id");
  if (!id.length()) {
    _server.send(400, "application/json", "{\"error\":\"missing id\"}");
    return;
  }
  char err[160] = "";
  if (!ThemeEngine::instance().remove(id.c_str(), err, sizeof(err))) {
    _server.send(400, "application/json", String("{\"error\":\"") + err + "\"}");
    return;
  }
  _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleThemesSelect() {
  if (!_server.hasArg("plain")) {
    _server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, _server.arg("plain"))) {
    _server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  const char* id = doc["id"] | "";
  if (!id[0]) {
    _server.send(400, "application/json", "{\"error\":\"missing id\"}");
    return;
  }
  ThemeEngine& te = ThemeEngine::instance();
  bool ok = false;
  int strip = doc["strip"] | -1;
  if (strip >= 0 && strip < kMaxStrips) {
    ok = te.applyStrip(strip, id);  // "" clears the per-strip override
  } else {
    ok = te.apply(id);
  }
  if (!ok) {
    _server.send(400, "application/json", "{\"error\":\"unknown theme\"}");
    return;
  }
  _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleThemesReset() {
  ThemeEngine::instance().resetDefaults();
  _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::sendDevices() {
  JsonDocument doc;
  doc["ok"] = true;
  _app->devices().jsonList(doc);
  JsonArray pf = doc["profiles"].to<JsonArray>();
  for (int i = 0; i < dev::builtinProfileCount(); ++i) {
    const dev::BuiltinProfileEntry* p = dev::builtinProfileAt(i);
    if (!p) continue;
    JsonObject o = pf.add<JsonObject>();
    o["id"] = p->id;
    o["name"] = p->model;
    o["manufacturer"] = p->manufacturer;
    o["deviceType"] = p->deviceType;
    o["capabilities"] = p->capabilities;
    o["needsConditioning"] = p->needsConditioning;
  }
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::handleDevicesScan() {
  int found = _app->devices().scan(millis());
  JsonDocument doc;
  doc["ok"] = true;
  doc["found"] = found;
  doc["registered"] = _app->devices().count();
  _app->devices().jsonList(doc);
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::handleDevice() {
  const String id = _server.arg("id");
  DeviceManager& dm = _app->devices();
  JsonDocument doc;

  if (_server.method() == HTTP_GET) {
    const dev::DeviceInfo* d = dm.registry().find(id.c_str());
    if (!d) {
      doc["ok"] = false;
      doc["error"] = "not_found";
      String out;
      serializeJson(doc, out);
      _server.send(404, "application/json", out);
      return;
    }
    // Reuse the list serializer, then pull this one row back out so the JSON
    // shape is identical for details and the list.
    dm.jsonList(doc);
    doc["ok"] = true;
    String out;
    serializeJson(doc, out);
    _server.send(200, "application/json", out);
    return;
  }

  if (_server.method() == HTTP_DELETE) {
    const String guarded = id;
    if (guarded.startsWith("local:") || guarded.startsWith("resonux:")) {
      doc["ok"] = false;
      doc["error"] = "builtin_row";
      String out;
      serializeJson(doc, out);
      _server.send(400, "application/json", out);
      return;
    }
    bool removed = dm.remove(id.c_str());
    doc["ok"] = removed;
    doc["deleted"] = removed;
    if (!removed) doc["error"] = "not_found";
    String out;
    serializeJson(doc, out);
    _server.send(removed ? 200 : 404, "application/json", out);
    return;
  }

  if (_server.method() == HTTP_POST) {
    JsonDocument body;
    if (deserializeJson(body, _server.arg("plain")) != DeserializationError::Ok) {
      doc["ok"] = false;
      doc["error"] = "bad_json";
      String out;
      serializeJson(doc, out);
      _server.send(400, "application/json", out);
      return;
    }
    const char* profileId =
        body["profileId"].is<const char*>() ? body["profileId"].as<const char*>()
                                            : nullptr;
    const char* name =
        body["name"].is<const char*>() ? body["name"].as<const char*>() : nullptr;
    const char* note =
        body["note"].is<const char*>() ? body["note"].as<const char*>() : nullptr;
    bool ok = true;
    if (profileId && profileId[0]) ok = ok && dm.identify(id.c_str(), profileId);
    if (name || note) ok = ok && dm.configure(id.c_str(), name, note);
    if (!ok) {
      doc["ok"] = false;
      doc["error"] = "bad_request";
      String out;
      serializeJson(doc, out);
      _server.send(400, "application/json", out);
      return;
    }
    doc["ok"] = true;
    String out;
    serializeJson(doc, out);
    _server.send(200, "application/json", out);
    return;
  }

  _server.send(405, "application/json", "{\"ok\":false,\"error\":\"method\"}");
}

void WebUi::sendAudioSources() {
  const Config& cfg = _app->config();
  const bool slave = cfg.sync.enabled && cfg.sync.role == SYNC_SLAVE;

  JsonDocument doc;
  doc["ok"] = true;
  doc["current"] = cfg.audioSource;
  doc["autoSelect"] = cfg.autoSelectSource;
  doc["preferred"] = cfg.preferredSource;
  doc["fallback"] = cfg.fallbackSource;

  JsonArray opts = doc["sources"].to<JsonArray>();
  for (int k = SOURCE_NONE; k < SOURCE_COUNT; ++k) {
    bool available;
    switch (k) {
      case SOURCE_NONE:
      case SOURCE_TEST:
        available = true;
        break;
      case SOURCE_MIC:
        available = !slave;
        break;
      default:
        available = false;  // reserved until a device binding exists
        break;
    }
    JsonObject o = opts.add<JsonObject>();
    o["id"] = k;
    o["ident"] = sourceKindIdent((SourceKind)k);
    o["label"] = sourceKindLabel((SourceKind)k);
    o["available"] = available;
    o["current"] = (k == cfg.audioSource);
  }
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}

void WebUi::handleAudioSource() {
  JsonDocument doc;
  JsonDocument body;
  if (deserializeJson(body, _server.arg("plain")) != DeserializationError::Ok) {
    doc["ok"] = false;
    doc["error"] = "bad_json";
    String out;
    serializeJson(doc, out);
    _server.send(400, "application/json", out);
    return;
  }
  bool ok = true;
  if (body["source"].is<int>()) ok = ok && _app->setAudioSource(body["source"]);
  if (body["autoSelect"].is<bool>()) ok = ok && _app->setAutoSelect(body["autoSelect"]);
  if (body["preferredSource"].is<int>()) ok = ok && _app->setPreferredSource(body["preferredSource"]);
  if (!ok) {
    doc["ok"] = false;
    doc["error"] = "bad_value";
    String out;
    serializeJson(doc, out);
    _server.send(400, "application/json", out);
    return;
  }
  doc["ok"] = true;
  doc["source"] = _app->audioSource();
  doc["autoSelect"] = _app->config().autoSelectSource;
  doc["preferred"] = _app->preferredSource();
  doc["fallback"] = _app->config().fallbackSource;
  String out;
  serializeJson(doc, out);
  _server.send(200, "application/json", out);
}