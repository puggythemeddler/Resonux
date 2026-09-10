#pragma once
#include "config/Config.h"
#include "device/DeviceRegistry.h"
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <cstdint>
#include <cstdio>

using dev::DeviceInfo;
using dev::DeviceRegistry;

// Phase 9 device mesh: every node answers a RESO_DISCOVER multicast probe and
// can scan for peers, building a live universal-detection registry. The
// registry is persisted (trusted/configured rows) to LittleFS `devices.json`.
// Local hardware is projected into the same registry so the web UI and audio
// source selector speak one vocabulary for all sources.

// Discovery transport (distinct from the sync multicast group/port).
inline constexpr uint16_t kDiscoverPort = 9770;
inline constexpr char kDiscoverGroup[] = "239.255.42.10";
inline constexpr char kDiscoverProbe[] = "RESO_DISCOVER v1\n";
inline constexpr char kDiscoverResp[] = "RESO-DISCOVER-RESP";

class DeviceStore {
 public:
  static constexpr const char* kPath = "/devices.json";

  bool begin();                       // ensures mount means LittleFS is ready
  bool load(DeviceRegistry& reg);     // merges persisted rows (validated)
  bool save(const DeviceRegistry& reg) const;  // writes persisted rows only

 private:
  bool _ok = false;
};

class DeviceManager {
 public:
  DeviceManager();

  void begin(const Config& cfg);       // load persisted, project local rows
  void tick(uint32_t nowMs);           // answer probes, refresh local lastSeen
  int  scan(uint32_t nowMs);           // one synchronous discovery sweep

  const DeviceRegistry& registry() const { return _registry; }
  DeviceInfo* find(const char* id);
  int count() const { return _registry.count(); }

  // Identity/binding: name the discovery row with a validated builtin profile.
  bool identify(const char* id, const char* profileId);
  bool configure(const char* id, const char* name, const char* note);
  bool remove(const char* id);

  void jsonList(JsonDocument& doc) const;
  bool persistenceOk() const { return _storeOk; }
  bool responderUp() const { return _responderUp; }

 private:
  void projectLocal(const Config& cfg);
  bool processInbound(uint32_t nowMs);
  bool persistLocked() { return _store.save(_registry); }

  DeviceRegistry _registry;
  DeviceStore _store;
  WiFiUDP _udp;
  bool _responderUp = false;
  bool _storeOk = false;
  uint8_t _rxBuf[512];
};