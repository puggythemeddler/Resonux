#include "device/DeviceManager.h"
#include "device/DeviceProfile.h"
#include "util/Log.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <string.h>

using namespace dev;

namespace {
int clampTo(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

dev::ConnectionType connectionFromIdent(const char* id) {
  for (int i = 0; i < dev::kConnectionCount; ++i) {
    if (strcmp(dev::connectionAt(i).id, id) == 0) {
      return dev::connectionAt(i).kind;
    }
  }
  return CONN_UNKNOWN;
}
}  // namespace

DeviceManager::DeviceManager() = default;

// ------------------------------------------------------------------ persistence

bool DeviceStore::begin() {
  _ok = LittleFS.begin(true);
  if (!_ok) {
    logBoot("device", "LittleFS unavailable - device rows volatile");
  }
  return _ok;
}

static bool validProfileId(const char* id) {
  return id && id[0] && dev::profileById(id).id[0];
}

bool DeviceStore::load(DeviceRegistry& reg) {
  if (!_ok || !LittleFS.exists(kPath)) return false;
  File f = LittleFS.open(kPath, "r");
  if (!f || f.size() == 0) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logBoot("device", "store corrupt - starting fresh");
    return false;
  }

  JsonArray rows = doc["devices"].as<JsonArray>();
  if (rows.isNull()) return false;
  int loaded = 0;
  for (JsonObject o : rows) {
    DeviceInfo d;
    strncpy(d.id, (const char*)(o["id"] | ""), sizeof(d.id) - 1);
    if (!d.id[0]) continue;
    strncpy(d.name, (const char*)(o["name"] | ""), sizeof(d.name) - 1);
    const char* pid = o["profileId"] | "";
    if (validProfileId(pid)) {
      strncpy(d.profileId, pid, sizeof(d.profileId) - 1);
    }
    d.capabilities = o["capabilities"] | 0u;
    d.connection = (ConnectionType)clampTo(
        o["connection"] | (int)CONN_UNKNOWN, 0, CONN_COUNT - 1);
    d.status = (DeviceStatus)clampTo(o["status"] | (int)STATUS_DETECTED, 0,
                                     STATUS_COUNT - 1);
    d.source = (DiscoverySource)clampTo(o["source"] | (int)SRC_UNKNOWN, 0,
                                        SRC_COUNT - 1);
    d.persisted = o["persisted"] | false;
    d.needsConditioning = o["needsConditioning"] | false;
    strncpy(d.note, (const char*)(o["note"] | ""), sizeof(d.note) - 1);
    d.lastSeenMs = 0;
    if (reg.upsert(d) >= 0) ++loaded;
  }
  logBoot("device", "store loaded: %d rows", loaded);
  return loaded > 0;
}

bool DeviceStore::save(const DeviceRegistry& reg) const {
  if (!_ok) return false;
  JsonDocument doc;
  JsonArray rows = doc["devices"].to<JsonArray>();
  for (int i = 0; i < reg.count(); ++i) {
    const DeviceInfo* d = reg.at(i);
    if (!d || !d->persisted) continue;
    JsonObject o = rows.add<JsonObject>();
    o["id"] = d->id;
    o["name"] = d->name;
    o["profileId"] = d->profileId;
    o["capabilities"] = d->capabilities;
    o["connection"] = (int)d->connection;
    o["status"] = (int)d->status;
    o["source"] = (int)d->source;
    o["persisted"] = true;
    o["needsConditioning"] = d->needsConditioning;
    o["note"] = d->note;
  }
  File f = LittleFS.open(kPath, "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

// ------------------------------------------------------------------ discovery

void DeviceManager::begin(const Config& cfg) {
  _storeOk = _store.begin();
  _registry.clearTransient();
  if (_storeOk) _store.load(_registry);
  projectLocal(cfg);
  if (_storeOk) _store.save(_registry);

  // Responder socket: joined to the multicast group on 9770.
  if (WiFi.getMode() != WIFI_OFF) {
    _responderUp = _udp.beginMulticast(IPAddress(239, 255, 42, 10),
                                       kDiscoverPort) == 1;
    if (!_responderUp) {
      logBoot("device", "discovery responder bind failed");
    }
  }
  logBoot("device", "manager up: %d devices (persist=%d)", _registry.count(),
          _storeOk ? 1 : 0);
}

void DeviceManager::projectLocal(const Config& cfg) {
  DeviceInfo self;
  strncpy(self.id, "resonux:self", sizeof(self.id) - 1);
  strncpy(self.name, cfg.deviceName, sizeof(self.name) - 1);
  const dev::DeviceProfile sp = dev::profileById("resonux");
  if (sp.id[0]) {
    strncpy(self.profileId, sp.id, sizeof(self.profileId) - 1);
    self.capabilities = sp.capabilities;
  } else {
    self.capabilities =
        dev::CAP_NETWORK | dev::CAP_ARTNET | dev::CAP_LED_OUTPUT |
        dev::CAP_ADDRESSABLE_LED | dev::CAP_MICROPHONE | dev::CAP_AUDIO_INPUT;
  }
  self.connection = CONN_NETWORK;
  self.status = STATUS_CONFIGURED;
  self.source = SRC_NETWORK;
  self.persisted = true;
  self.lastSeenMs = millis();
  _registry.upsert(self);

  // The onboard addressable LED strip (present whenever a strip is built).
  DeviceInfo led;
  strncpy(led.id, "local:led", sizeof(led.id) - 1);
  strncpy(led.name, "Onboard LED strip", sizeof(led.name) - 1);
  led.capabilities = dev::CAP_LED_OUTPUT | dev::CAP_ADDRESSABLE_LED |
                     dev::CAP_RGB_PWM;
  led.connection = CONN_GPIO;
  led.status = cfg.stripCount > 0 ? STATUS_CONFIGURED : STATUS_DETECTED;
  led.source = SRC_LOCAL;
  led.persisted = true;
  led.lastSeenMs = millis();
  _registry.upsert(led);

  // The I2S microphone (unless running as a sync slave).
  bool micPresent =
      !(cfg.sync.enabled && cfg.sync.role == SYNC_SLAVE) && cfg.micData >= 0;
  DeviceInfo mic;
  strncpy(mic.id, "local:mic", sizeof(mic.id) - 1);
  strncpy(mic.name, "INMP441 microphone", sizeof(mic.name) - 1);
  const dev::DeviceProfile mp = dev::profileById("inmp441_mic");
  if (mp.id[0]) {
    strncpy(mic.profileId, mp.id, sizeof(mic.profileId) - 1);
    mic.capabilities = mp.capabilities;
  } else {
    mic.capabilities = dev::CAP_MICROPHONE | dev::CAP_AUDIO_INPUT;
  }
  mic.connection = CONN_GPIO;
  mic.status = micPresent ? STATUS_CONFIGURED : STATUS_DETECTED;
  mic.source = SRC_LOCAL;
  mic.persisted = true;
  mic.lastSeenMs = millis();
  _registry.upsert(mic);

  if (cfg.display.enabled) {
    DeviceInfo disp;
    strncpy(disp.id, "local:display", sizeof(disp.id) - 1);
    strncpy(disp.name, "Touch display panel", sizeof(disp.name) - 1);
    const dev::DeviceProfile tp = dev::profileById("touch_display");
    if (tp.id[0]) {
      strncpy(disp.profileId, tp.id, sizeof(disp.profileId) - 1);
      disp.capabilities = tp.capabilities;
    } else {
      disp.capabilities = dev::CAP_DISPLAY | dev::CAP_TOUCH | dev::CAP_SPI;
    }
    disp.connection = CONN_SPI_BUS;
    disp.status = STATUS_CONFIGURED;
    disp.source = SRC_LOCAL;
    disp.persisted = true;
    disp.lastSeenMs = millis();
    _registry.upsert(disp);
  }
}

void DeviceManager::tick(uint32_t nowMs) {
  if (!_responderUp) return;
  processInbound(nowMs);
  // Keep local hardware rows fresh so "last seen" reflects boot time.
  if (_registry.find("local:mic")) {
    DeviceInfo* d = _registry.slot(_registry.indexOf("local:mic"));
    if (d) d->lastSeenMs = nowMs;
  }
}

// Parses one inbound datagram. Returns true when it updated/added a peer row.
bool DeviceManager::processInbound(uint32_t nowMs) {
  int len = _udp.parsePacket();
  if (len <= 0) return false;
  int n = _udp.read(_rxBuf, sizeof(_rxBuf) - 1);
  if (n <= 0) return false;
  _rxBuf[n] = '\0';

  const IPAddress remote = _udp.remoteIP();
  const uint16_t rport = _udp.remotePort();

  if (strncmp((char*)_rxBuf, kDiscoverProbe, 14) == 0) {
    // Distinguish multicast probes from our own (P2P unicast loop guard).
    if (remote == WiFi.localIP()) return false;
    // Answer the probe with a compact identity line back to the sender.
    if (_udp.beginPacket(remote, rport) == 1) {
      _udp.print(kDiscoverResp);
      _udp.print("id=resonux:self name=resonux kind=network ");
      _udp.print("caps=");
      _udp.print((unsigned long)(dev::CAP_NETWORK | dev::CAP_AUDIO_INPUT));
      _udp.print(" source=");
      _udp.print((int)SRC_NETWORK);
      _udp.endPacket();
    }
    return false;
  }

  if (strncmp((char*)_rxBuf, kDiscoverResp, 15) == 0) {
    char id[24] = "", name[24] = "", kind[16] = "";
    uint32_t caps = 0;
    int src = (int)SRC_UNKNOWN;
    const char* p = (const char*)_rxBuf + 15;
    while (p && *p) {
      while (*p == ' ') ++p;
      const char* nl = strchr(p, ' ');
      size_t tl = nl ? (size_t)(nl - p) : strlen(p);
      const char* eq = strchr(p, '=');
      if (!eq) break;
      size_t kl = (size_t)(eq - p);
      if (tl <= 32) {
        if (kl == 2 && strncmp(p, "id=", 3) == 0) {
          strncpy(id, eq + 1, sizeof(id) - 1);
        } else if (kl == 4 && strncmp(p, "name=", 5) == 0) {
          strncpy(name, eq + 1, sizeof(name) - 1);
        } else if (kl == 4 && strncmp(p, "kind=", 5) == 0) {
          strncpy(kind, eq + 1, sizeof(kind) - 1);
        } else if (kl == 4 && strncmp(p, "caps=", 5) == 0) {
          caps = (uint32_t)strtoul(eq + 1, nullptr, 10);
        } else if (kl == 6 && strncmp(p, "source=", 7) == 0) {
          src = clampTo(atoi(eq + 1), 0, SRC_COUNT - 1);
        }
      }
      p = nl;
    }
    if (!id[0]) return false;
    DeviceInfo d;
    strncpy(d.id, id, sizeof(d.id) - 1);
    strncpy(d.name, name[0] ? name : id, sizeof(d.name) - 1);
    dev::ConnectionType conn = connectionFromIdent(kind);
    d.connection = conn == CONN_UNKNOWN ? CONN_NETWORK : conn;
    d.capabilities = caps;
    d.status = STATUS_DETECTED;
    d.source = (DiscoverySource)(src >= 0 ? src : SRC_NETWORK);
    d.persisted = false;  // discovery is transient until an explicit identity
    d.lastSeenMs = nowMs;
    int idx = _registry.upsert(d);
    return idx >= 0;
  }
  return false;
}

int DeviceManager::scan(uint32_t nowMs) {
  if (!_responderUp) return 0;

  // Probe: multicast + broadcast, then collect replies for ~700 ms.
  if (_udp.beginPacket(IPAddress(239, 255, 42, 10), kDiscoverPort) == 1) {
    _udp.write((const uint8_t*)kDiscoverProbe, strnlen(kDiscoverProbe, 40));
    _udp.endPacket();
  }
  if (_udp.beginPacket(IPAddress(255, 255, 255, 255), kDiscoverPort) == 1) {
    _udp.write((const uint8_t*)kDiscoverProbe, strnlen(kDiscoverProbe, 40));
    _udp.endPacket();
  }

  int got = 0;
  const uint32_t done = nowMs + 700;
  while ((int32_t)(millis() - done) < 0 && got < 8) {
    if (processInbound(millis())) ++got;
    delay(5);
  }
  return got;
}

DeviceInfo* DeviceManager::find(const char* id) {
  return _registry.slot(_registry.indexOf(id));
}

bool DeviceManager::identify(const char* id, const char* profileId) {
  const dev::DeviceProfile p = dev::profileById(profileId);
  if (!p.id[0]) return false;
  DeviceInfo* d = find(id);
  if (!d) return false;

  strncpy(d->profileId, p.id, sizeof(d->profileId) - 1);
  d->capabilities = p.capabilities;
  d->needsConditioning = p.needsConditioning;
  dev::DeviceMatch m = dev::classify(*d);
  d->status = m.status;
  d->persisted = true;  // a named identity is trusted until explicitly removed
  return _storeOk ? _store.save(_registry) : true;
}

bool DeviceManager::configure(const char* id, const char* name,
                              const char* note) {
  DeviceInfo* d = find(id);
  if (!d) return false;
  if (name) strncpy(d->name, name, sizeof(d->name) - 1);
  if (note) strncpy(d->note, note, sizeof(d->note) - 1);
  return _storeOk ? _store.save(_registry) : true;
}

bool DeviceManager::remove(const char* id) {
  if (!_registry.remove(id)) return false;
  return _storeOk ? _store.save(_registry) : true;
}

void DeviceManager::jsonList(JsonDocument& doc) const {
  JsonArray out = doc["devices"].to<JsonArray>();
  for (int i = 0; i < _registry.count(); ++i) {
    const DeviceInfo* d = _registry.at(i);
    if (!d) continue;
    JsonObject o = out.add<JsonObject>();
    o["id"] = d->id;
    o["name"] = d->name;
    o["profileId"] = d->profileId;
    o["capabilities"] = d->capabilities;
    o["connection"] = dev::connectionIdent(d->connection);
    o["connectionLabel"] = dev::connectionLabel(d->connection);
    o["status"] = dev::statusIdent(d->status);
    o["statusLabel"] = dev::statusLabel(d->status);
    o["source"] = d->source;
    o["persisted"] = d->persisted;
    o["needsConditioning"] = d->needsConditioning;
    o["note"] = d->note;
    o["lastSeen"] = d->lastSeenMs;
    JsonArray caps = o["caps"].to<JsonArray>();
    for (int c = 0; c < dev::kCapabilityCount; ++c) {
      if (hasCapability(*d, (dev::Capability)(1u << c))) {
        caps.add(dev::capabilityAt(c).id);
      }
    }
  }
}