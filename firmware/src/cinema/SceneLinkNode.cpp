#include "cinema/SceneLinkNode.h"
#include "util/Log.h"
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

SceneLinkNode::~SceneLinkNode() { stop(); }

bool SceneLinkNode::begin(const cine::Config& cfg) {
  stop();
  if (!cfg.receiveUdp) return false;

  WiFiMode_t mode = WiFi.getMode();
  if (mode == WIFI_OFF) {
    logBoot("scene", "disabled - network off");
    return false;
  }

  strncpy(_group, cfg.group, sizeof(_group) - 1);
  _group[sizeof(_group) - 1] = '\0';
  _port = cfg.port;
  _picker.configure(cfg.staleMs);

  if (!_udp.begin(_port)) {
    logBoot("scene", "udp bind failed on %u", _port);
    return false;
  }

  IPAddress group;
  if (!group.fromString(_group) || !_udp.beginMulticast(group, _port)) {
    logBoot("scene", "multicast join failed (%s)", _group);
    _udp.stop();
    return false;
  }

  _mutex = xSemaphoreCreateMutex();
  if (!_mutex) {
    _udp.stop();
    return false;
  }

  xTaskCreatePinnedToCore(SceneLinkNode::taskEntry, "scene", kTaskStack, this,
                          kTaskPriority, &_task, 0);
  _enabled = true;
  logBoot("scene", "listening %s:%u", _group, _port);
  return true;
}

void SceneLinkNode::stop() {
  _running = false;
  if (_task) {
    // taskLoop() exits on the next 2 ms iteration and the task deletes itself
    for (int i = 0; i < 25 && _task; ++i) vTaskDelay(pdMS_TO_TICKS(5));
    _task = nullptr;
  }
  _udp.stop();
  if (_mutex) {
    vSemaphoreDelete(_mutex);
    _mutex = nullptr;
  }
  _enabled = false;
  _have = false;
  _haveSpatial = false;
  _seq = 0;
  _lastRxMs = 0;
  _picker.reset();
}

bool SceneLinkNode::sourceLabel(char* out, size_t cap) const {
  if (!out || cap == 0) return false;
  const cine::SourceInfo* a = _picker.active();
  if (!a) {
    out[0] = '\0';
    return false;
  }
  const uint32_t ip = a->key.ip;
  snprintf(out, cap, "%u.%u.%u.%u:%u", ip >> 24, (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF, ip & 0xFF, a->key.port);
  return true;
}

bool SceneLinkNode::live(uint32_t staleMs) const {
  if (!_enabled || !_have || staleMs == 0) return false;
  return (uint32_t)(millis() - _lastRxMs) < staleMs;
}

bool SceneLinkNode::frame(sceneframe::Frame& out) const {
  if (!_mutex) return false;
  if (!xSemaphoreTake(_mutex, pdMS_TO_TICKS(5))) return false;
  const bool ok = _have;
  if (ok) out = _latest;
  xSemaphoreGive(_mutex);
  return ok;
}

bool SceneLinkNode::spatial(sceneframe::SpatialInfo& out) const {
  if (!_mutex) return false;
  if (!xSemaphoreTake(_mutex, pdMS_TO_TICKS(5))) return false;
  const bool ok = _haveSpatial;
  if (ok) out = _spatial;
  xSemaphoreGive(_mutex);
  return ok;
}

void SceneLinkNode::taskEntry(void* arg) {
  SceneLinkNode* node = (SceneLinkNode*)arg;
  node->_running = true;
  while (node->_running) node->taskLoop();
  vTaskDelete(nullptr);  // stop() clears _running; the task cleans itself up
}

void SceneLinkNode::taskLoop() {
  if (!_running) return;
  int len = _udp.parsePacket();
  if (len > 0) {
    const IPAddress rip = _udp.remoteIP();
    const uint16_t rport = _udp.remotePort();
    size_t cap = sizeof(_rxBuf);
    int n = _udp.read(_rxBuf, cap);
    sceneframe::Frame p;
    memcpy(&p, _rxBuf, sizeof(p));
    if (sceneframe::validFrame(p, (size_t)n)) {
      // Source identity: only frames from the active source (or a justified
      // switch to it) are stored; a second companion on the same group is
      // tracked but never committed while the current one is live.
      cine::SourceKey key;
      key.ip = (uint32_t)rip;
      key.port = rport;
      const cine::SourceAccept accepted =
          _picker.accept(key, p.seq, p.hostTimeMs, millis());
      if (accepted == cine::ACCEPT_COMMIT) {
        // optional appended spatial block (SpatialBlock.h). Over-long payloads
        // are accepted; the block is skipped entirely if absent or malformed.
        sceneframe::SpatialInfo sp;
        const bool hasSpatial =
            (p.flags & sceneframe::SFLAG_SPATIAL_BLOCK) != 0 &&
            sceneframe::parseSpatial(_rxBuf + sizeof(p),
                                     (size_t)n - sizeof(p), sp);
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5))) {
          _latest = p;
          _have = true;
          if (hasSpatial) {
            _spatial = sp;
            _haveSpatial = true;
          }
          _lastRxMs = millis();
          _seq = p.seq;
          xSemaphoreGive(_mutex);
        }
      }
    }
  }
  vTaskDelay(2 / portTICK_PERIOD_MS);
}