#include "network/WifiManager.h"
#include "util/Log.h"
#include <Arduino.h>
#include <WiFi.h>

WifiManager& WifiManager::instance() {
  static WifiManager mgr;
  return mgr;
}

bool WifiManager::begin(const NetworkConfig& cfg) {
  _cfg = cfg;
  if (!cfg.enabled) {
    _lastError = "disabled";
    return false;
  }

  if (cfg.mode == NET_AP_ONLY) {
    _managed = startAp();
  } else if (cfg.mode == NET_STA_ONLY) {
    _managed = connectSta();
  } else {
    _managed = connectSta();
    if (!_connected) _apMode = startAp();
  }
  return _managed;
}

bool WifiManager::ensureConnected() {
  if (!_managed) return false;
  if (_connected) return true;

  if (_apMode) return true;  // AP never drops (no STA reconnect needed)

  if (connectSta()) return true;

  if (_cfg.mode == NET_AP_STA_FALLBACK && !_apMode) {
    _apMode = startAp();
  }
  return _connected || _apMode;
}

bool WifiManager::connectSta() {
  if (!_cfg.staSsid[0]) {
    _lastError = "no STA SSID";
    _connected = false;
    return false;
  }

  logBoot("wifi", "connecting to %s...", _cfg.staSsid);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  WiFi.begin(_cfg.staSsid, _cfg.staPassword);

  uint32_t start = millis();
  while (millis() - start < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      _connected = true;
      _apMode = false;
      logBoot("wifi", "STA OK ip=%s", WiFi.localIP().toString().c_str());
      return true;
    }
    delay(100);
  }

  _connected = false;
  _lastError = "STA timeout";
  logBoot("wifi", "STA FAILED");
  return false;
}

bool WifiManager::startAp() {
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(_cfg.apSsid, _cfg.apPassword);
  if (!ok) {
    _lastError = "AP failed";
    logBoot("wifi", "AP FAILED");
    return false;
  }
  _connected = true;
  _apMode = true;
  logBoot("wifi", "AP:%s ip=%s", _cfg.apSsid,
          WiFi.softAPIP().toString().c_str());
  return true;
}

IPAddress WifiManager::ip() const {
  if (_apMode) return WiFi.softAPIP();
  return WiFi.localIP();
}

IPAddress WifiManager::gateway() const {
  if (_apMode) return WiFi.softAPIP();
  return WiFi.gatewayIP();
}

const char* WifiManager::modeName() const {
  if (!_managed) return "off";
  if (_apMode) return "AP";
  return "STA";
}