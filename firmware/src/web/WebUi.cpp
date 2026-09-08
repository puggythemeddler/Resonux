#include "web/WebUi.h"
#include "audio/AudioFrame.h"
#include "config/ConfigStore.h"
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
  _server.on("/api/themes", HTTP_GET, [this]() { sendThemes(); });
  _server.on("/api/themes", HTTP_PUT, [this]() { handleThemesPut(); });
  _server.on("/api/themes", HTTP_DELETE, [this]() { handleThemesDelete(); });
  _server.on("/api/themes/select", HTTP_POST, [this]() { handleThemesSelect(); });
  _server.on("/api/themes/reset", HTTP_POST, [this]() { handleThemesReset(); });
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
  _server.send(200, "application/json", "{\"ok\":true}");
  logBoot("web", "reboot requested from dashboard");
  delay(300);
  ESP.restart();
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