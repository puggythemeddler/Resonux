#pragma once
#include "config/Config.h"
#include "sync/SyncClock.h"
#include "sync/SyncProtocol.h"
#include <WiFiUdp.h>
#include <cstdint>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Phase 9 multi-controller sync.
//   MASTER: audio task calls publishFrame(); this node broadcasts the newest
//           AudioFrame to a multicast group ~heartbeatMs apart.
//   SLAVE:  joins the group; each fresh packet is clock-compensated and handed
//           to the onFrame callback (App injects it as the live AudioFrame).
// The same code compiles in both roles; select via config `sync.role`.

class SyncNode {
public:
  SyncNode() = default;
  ~SyncNode();

  bool begin(const SyncConfig& cfg,
             const std::function<void(const AudioFrame&)>& onFrame);
  void stop();

  bool enabled() const { return _enabled; }
  int  role() const { return _role; }
  const char* roleName() const;
  uint32_t seq() const { return _seq; }              // last rx seq (slave)
  uint32_t lastPacketMs() const { return _lastPacketMs; }
  int32_t clockOffsetMs() const { return _clock.offsetMs(); }
  bool masterAlive() const;                          // slave: fresh rx within timeout

  // master only: hand over the newest analyzed frame
  void publishFrame(const AudioFrame& f);

private:
  void taskLoop();
  void taskLoopMaster();
  void taskLoopSlave();
  static void taskEntry(void* arg);

  SyncConfig _cfg;
  std::function<void(const AudioFrame&)> _onFrame;
  WiFiUDP _udp;
  TaskHandle_t _task = nullptr;
  bool _enabled = false;
  bool _running = false;
  int _role = SYNC_OFF;

  AudioFrame _tx = {};                  // master send buffer (mutex-guarded)
  uint32_t _pendingSeq = 0;             // frames published by audio task
  uint32_t _sentSeq = 0;                // last seq transmitted
  SemaphoreHandle_t _txMutex = nullptr;

  uint8_t _rxBuf[sizeof(syncpkt::Packet) + 64];  // byte-safe read buffer
  SyncClock _clock;
  uint32_t _seq = 0;
  uint32_t _lastPacketMs = 0;

  static constexpr uint32_t kTaskStack = 4096;
  static constexpr UBaseType_t kTaskPriority = 15;
};