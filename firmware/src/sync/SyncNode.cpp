#include "sync/SyncNode.h"
#include "util/Log.h"
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

SyncNode::~SyncNode() { stop(); }

const char* SyncNode::roleName() const {
  switch (_role) {
    case SYNC_MASTER: return "master";
    case SYNC_SLAVE: return "slave";
    default: return "off";
  }
}

bool SyncNode::begin(const SyncConfig& cfg,
                     const std::function<void(const AudioFrame&)>& onFrame) {
  _cfg = cfg;
  _onFrame = onFrame;
  _enabled = cfg.enabled && (cfg.role == SYNC_MASTER || cfg.role == SYNC_SLAVE);
  if (!_enabled) return false;
  _role = cfg.role;

  WiFiMode_t mode = WiFi.getMode();
  if (cfg.role == SYNC_SLAVE && mode == WIFI_OFF) {
    logBoot("sync", "disabled - network off");
    _enabled = false;
    return false;
  }

  if (!_udp.begin(_cfg.port)) {
    logBoot("sync", "udp bind failed on %u", _cfg.port);
    _enabled = false;
    return false;
  }

  if (_role == SYNC_SLAVE) {
    IPAddress group;
    if (!group.fromString(_cfg.group) ||
        !_udp.beginMulticast(group, _cfg.port)) {
      logBoot("sync", "multicast join failed (%s)", _cfg.group);
      _enabled = false;
      return false;
    }
  }

  if (_role == SYNC_MASTER) {
    _txMutex = xSemaphoreCreateMutex();
    if (!_txMutex) {
      _enabled = false;
      return false;
    }
  }

  xTaskCreatePinnedToCore(SyncNode::taskEntry, "sync", kTaskStack, this,
                          kTaskPriority, &_task, 0);
  logBoot("sync", "%s up: %s:%u hb=%ums timeout=%ums", roleName(), _cfg.group,
          _cfg.port, _cfg.heartbeatMs, _cfg.timeoutMs);
  return true;
}

void SyncNode::stop() {
  _running = false;
  if (_task) {
    vTaskDelay(pdMS_TO_TICKS(30));
    _task = nullptr;
  }
  _udp.stop();
  if (_txMutex) {
    vSemaphoreDelete(_txMutex);
    _txMutex = nullptr;
  }
  _enabled = false;
}

bool SyncNode::masterAlive() const {
  if (_role != SYNC_SLAVE || _lastPacketMs == 0) return false;
  return (uint32_t)(millis() - _lastPacketMs) < _cfg.timeoutMs;
}

void SyncNode::publishFrame(const AudioFrame& f) {
  if (_role != SYNC_MASTER || !_txMutex) return;
  if (!xSemaphoreTake(_txMutex, pdMS_TO_TICKS(2))) return;
  _tx = f;
  ++_pendingSeq;
  xSemaphoreGive(_txMutex);
}

void SyncNode::taskEntry(void* arg) {
  SyncNode* node = (SyncNode*)arg;
  node->_running = true;
  for (;;) node->taskLoop();
}

void SyncNode::taskLoop() {
  if (_role == SYNC_MASTER)
    taskLoopMaster();
  else
    taskLoopSlave();
}

void SyncNode::taskLoopMaster() {
  AudioFrame f;
  uint32_t seq;
  bool have = false;
  if (_txMutex && xSemaphoreTake(_txMutex, pdMS_TO_TICKS(2))) {
    if (_pendingSeq != _sentSeq) {
      f = _tx;
      seq = _pendingSeq;
      have = true;
    }
    xSemaphoreGive(_txMutex);
  }
  if (have) {
    syncpkt::Packet p;
    syncpkt::packPacket(p, f, seq);
    if (_udp.beginPacket(_cfg.group, _cfg.port) == 1) {
      _udp.write((const uint8_t*)&p, sizeof(p));
      _udp.endPacket();
      _sentSeq = seq;
    }
  }
  uint32_t hb = _cfg.heartbeatMs ? _cfg.heartbeatMs : 32;
  vTaskDelay(hb / portTICK_PERIOD_MS);
}

void SyncNode::taskLoopSlave() {
  int len = _udp.parsePacket();
  if (len > 0) {
    int n = _udp.read(_rxBuf, sizeof(_rxBuf));
    syncpkt::Packet p;
    memcpy(&p, _rxBuf, sizeof(p));
    if (syncpkt::validPacket(p, (size_t)n)) {
      _clock.observe(millis(), p.masterTime);
      if (p.seq != _seq) {
        _seq = p.seq;
        _lastPacketMs = millis();
        AudioFrame f;
        syncpkt::unpackPacket(f, p);
        f.timeMs = _clock.toLocal(f.timeMs);
        if (_onFrame) _onFrame(f);
      }
    }
  }
  vTaskDelay(2 / portTICK_PERIOD_MS);
}