#pragma once
#include "cinema/CinematicConfig.h"
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

  uint8_t _rxBuf[sizeof(sceneframe::Frame) + sceneframe::kSpatialMaxBlock];

  static constexpr uint32_t kTaskStack = 3072;
  static constexpr UBaseType_t kTaskPriority = 14;
};