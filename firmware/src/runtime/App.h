#pragma once
#include "artnet/ArtNetNode.h"
#include "audio/AudioAnalyzer.h"
#include "audio/AudioFrame.h"
#include "audio/AudioSource.h"
#include "config/Config.h"
#include "display/DisplayManager.h"
#include "network/WifiManager.h"
#include "runtime/StripRuntime.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdint.h>

class WebUi;
class SyncNode;

class App {
public:
  static App& instance();

  bool begin();

  const Config& config() const { return _config; }
  uint32_t audioFrames() const { return _framesCount; }
  float audioFps() const { return _analyzer ? _analyzer->fps() : 0.0f; }
  const ArtNetNode* artnet() const { return _artnet; }
  const SyncNode* sync() const { return _sync; }
  const char* wifiModeName() const { return WifiManager::instance().modeName(); }

  bool takeFrame(AudioFrame& out);
  bool setMasterBrightness(uint8_t value);   // live, persists (no reboot)
  bool setStripEffect(int strip, int effectId); // live switch, persists
  bool setTheme(const char* id);             // live global theme switch, persists
  uint8_t masterBrightness() const { return _config.masterBrightness; }
  const DisplayManager& display() const { return DisplayManager::instance(); }

private:
  App() {}
  App(const App&) = delete;

  void buildStrips();
  void audioLoop();
  void ledLoop();
  void injectRemoteFrame(const AudioFrame& f);

  static void audioTaskEntry(void* arg);
  static void ledTaskEntry(void* arg);

  Config          _config;
  AudioSource*    _source = nullptr;
  AudioAnalyzer*  _analyzer = nullptr;
  StripRuntime*   _strips[kMaxStrips] = {nullptr};
  int             _stripCount = 0;

  AudioFrame      _frameStorage;
  SemaphoreHandle_t _mutex = nullptr;
  uint32_t        _framesCount = 0;
  uint32_t        _lastSeenFrame = 0;

  ArtNetNode*     _artnet = nullptr;
  SyncNode*       _sync = nullptr;
  WebUi*          _web = nullptr;

  TaskHandle_t    _audioTask = nullptr;
  TaskHandle_t    _ledTask = nullptr;
};