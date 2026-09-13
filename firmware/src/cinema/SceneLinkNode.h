#pragma once
#include "cinema/CinematicConfig.h"
#include "cinema/CompanionPicker.h"
#include "cinema/SceneFrame.h"
#include "cinema/SpatialBlock.h"
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdint.h>

// Cinematic companion receiver (spec §28). Listens on the SceneFrame multicast
// group and hands the freshest decoded frame to the LED task through a
// mutex-guarded copy. Receive-only by design — the ESP32 never emits SceneFrames.
//
// Source identity: a CompanionPicker keys each datagram by UDP src ip:port, so
// frames from a live-but-not-selected companion are tracked but never stored,
// and the active source only switches after it goes stale (CompanionPicker.h).
//
// Failsafe: the LED task checks `live(staleMs)`; if the companion goes quiet the
// engine falls back to pure on-device audio, so a dead companion can never
// freeze the lights.

class SceneLinkNode {
public:
  SceneLinkNode() = default;
  ~SceneLinkNode();

  bool begin(const cine::Config& cfg);
  void stop();

  bool enabled() const { return _enabled; }
  bool have() const { return _have; }
  bool live(uint32_t staleMs) const;
  // Copy of the latest valid frame; returns false when nothing is stored yet.
  bool frame(sceneframe::Frame& out) const;
  // Copy of the latest spatial block parsed off a frame payload. Returns
  // false when the last accepted frame carried none.
  bool spatial(sceneframe::SpatialInfo& out) const;
  uint32_t lastRxMs() const { return _lastRxMs; }
  uint32_t seq() const { return _seq; }

  // ---- source identity (CompanionPicker status for the web/UI) -------------
  const cine::CompanionPicker& picker() const { return _picker; }
  // Last accepted source, formatted "a.b.c.d:port" (false when idle).
  bool sourceLabel(char* out, size_t cap) const;
  uint32_t lastSourceIp() const {
    const cine::SourceInfo* a = _picker.active();
    return a ? a->key.ip : 0;
  }
  uint16_t lastSourcePort() const {
    const cine::SourceInfo* a = _picker.active();
    return a ? a->key.port : 0;
  }

private:
  void taskLoop();
  static void taskEntry(void* arg);

  WiFiUDP _udp;
  TaskHandle_t _task = nullptr;
  bool _enabled = false;
  bool _running = false;

  char _group[16] = "";
  uint16_t _port = 0;

  sceneframe::Frame _latest;
  sceneframe::SpatialInfo _spatial;
  bool _haveSpatial = false;
  mutable SemaphoreHandle_t _mutex = nullptr;
  bool _have = false;
  uint32_t _seq = 0;
  uint32_t _lastRxMs = 0;
  cine::CompanionPicker _picker;

  uint8_t _rxBuf[sizeof(sceneframe::Frame) + sceneframe::kSpatialMaxBlock];

  static constexpr uint32_t kTaskStack = 3072;
  static constexpr UBaseType_t kTaskPriority = 14;
};