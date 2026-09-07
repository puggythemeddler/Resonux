#pragma once
#include "config/Config.h"
#include <IPAddress.h>
#include <cstdint>

class WifiManager {
public:
  static WifiManager& instance();

  bool begin(const NetworkConfig& cfg);
  bool ensureConnected();
  bool connected() const { return _connected; }
  bool managed() const { return _managed; }
  IPAddress ip() const;
  IPAddress gateway() const;
  const char* modeName() const;
  const char* lastError() const { return _lastError; }

  bool isApMode() const { return _apMode; }

private:
  WifiManager() {}

  bool connectSta();
  bool startAp();

  bool _managed = false;
  bool _apMode = false;
  bool _connected = false;
  NetworkConfig _cfg;
  const char* _lastError = "idle";
};