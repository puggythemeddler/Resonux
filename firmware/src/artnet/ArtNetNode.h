#pragma once
#include "artnet/ArtNetProtocol.h"
#include "artnet/FixtureProfile.h"
#include "audio/AudioFrame.h"
#include "config/Config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFiUdp.h>
#include <cstdint>

class ArtNetNode {
public:
  ArtNetNode(const ArtNetConfig& cfg, const FixtureConfig* fixtures,
             int fixtureCount);
  ~ArtNetNode();

  bool begin();
  void stop();

  // Zero a final DMX frame and push it before stop(); fixtures fade to black
  // instead of holding the last audio-mapped values.
  void blackout();

  bool isConnected() const { return _connected; }
  const uint8_t* dmxData() const { return _dmxOut; }
  uint16_t dmxLength() const { return kDmxChannels; }

  void setAudioFrame(const AudioFrame& f);
  void applyAudioMapping();

  const char* statusString() const;

private:
  void taskLoop();
  static void taskEntry(void* arg);

  bool connectWifi();
  void handlePacket(const uint8_t* data, int len, const IPAddress& fromIp);
  void handlePoll(const IPAddress& fromIp);
  void handleDmx(const uint8_t* data, int len);
  void sendPollReply(const IPAddress& toIp);
  void sendDmx();

  void mapFixture(const AudioFrame& frame, int fixtureIdx,
                  const FixtureProfile& profile);
  static uint8_t scaleU8(float v);
  static uint8_t clamp255(int v);
  static void hsvToRgb(float h, float s, float v, float& r, float& g, float& b);

  const ArtNetConfig& _cfg;
  const FixtureConfig* _fixtures;
  int _fixtureCount;

  WiFiUDP _udp;
  TaskHandle_t _task = nullptr;
  bool _connected = false;
  bool _running = false;

  uint8_t _dmxIn[kDmxChannels] = {};
  uint8_t _dmxOut[kDmxChannels] = {};
  uint8_t _sequence = 0;

  AudioFrame _audio = {};
  SemaphoreHandle_t _audioMutex = nullptr;

  static constexpr uint32_t kTaskStack = 8192;
  static constexpr UBaseType_t kTaskPriority = 15;
};
