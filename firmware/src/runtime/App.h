#pragma once
#include "artnet/ArtNetNode.h"
#include "audio/AudioAnalyzer.h"
#include "audio/AudioFrame.h"
#include "audio/AudioSource.h"
#include "cinema/CinematicEngine.h"
#include "cinema/SceneAnalyzer.h"
#include "device/DeviceManager.h"
#include "config/Config.h"
#include "display/DisplayManager.h"
#include "network/WifiManager.h"
#include "runtime/StripRuntime.h"
#include "system/SystemMode.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdint.h>

class WebUi;
class SyncNode;
class SceneLinkNode;

class App {
public:
  static App& instance();

  bool begin();

  const Config& config() const { return _config; }
  Config&       config() { return _config; }
  uint32_t audioFrames() const { return _framesCount; }
  float audioFps() const { return _analyzer ? _analyzer->fps() : 0.0f; }
  const ArtNetNode* artnet() const { return _artnet; }
  const SyncNode* sync() const { return _sync; }
  const char* wifiModeName() const { return WifiManager::instance().modeName(); }

  DeviceManager& devices() { return _devices; }
  int  audioSource() const { return _config.audioSource; }
  bool setAudioSource(int kind);             // persists; applied at next boot
  int  preferredSource() const { return _config.preferredSource; }
  bool setPreferredSource(int kind);         // persists (no reboot semantics)
  bool setAutoSelect(bool on);               // persists
  int  activeAudioSource() const { return _resolvedSource; }
  const char* audioSourceReason() const { return _sourceReason; }

  bool takeFrame(AudioFrame& out);
  bool setMasterBrightness(uint8_t value);   // live, persists (no reboot)
  bool setStripEffect(int strip, int effectId); // live switch, persists
  bool setTheme(const char* id);             // live global theme switch, persists
  bool setSensitivity(float value);          // live per-strip, persists (no reboot)
  uint8_t masterBrightness() const { return _config.masterBrightness; }
  const DisplayManager& display() const { return DisplayManager::instance(); }

  // ---- system control -------------------------------------------------
  bool requestRestart();                   // graceful: quiet outputs, save, reboot
  bool requestPowerOff();                  // graceful: quiet outputs, save, deep sleep
  const sys::SystemMode& systemMode() const { return _sysMode; }
  const char* systemStateName() const { return _sysMode.stateName(); }

  bool setDisplayBacklightPct(int pct);    // 0-100, live + persists (no reboot)
  bool setDisplayTimeout(int seconds);     // 0 = always on, live + persists

  // ---- Cinematic Mode ----------------------------------------------------
  const cine::Status& cinematicStatus() const { return _cinStatus; }
  const cine::Look& cinematicLook() const { return _cinEngine.look(); }
  bool cinematicActive() const { return _config.cinematic.enabled; }
  bool cameraUdpLive() const;              // companion feed fresh right now
  bool setCinematicConfig(const cine::Config& c);  // clamps, applies, mirrors; save is debounced
  void flushPendingSave();                          // fire the debounced config save, if due

private:
  App() {}
  App(const App&) = delete;

  void buildStrips();
  void audioLoop();
  void ledLoop();
  void injectRemoteFrame(const AudioFrame& f);
  void extinguishLeds();
  void gracefulShutdown();
  bool startShutdown(sys::Action a);

  static void audioTaskEntry(void* arg);
  static void ledTaskEntry(void* arg);
  static void shutdownTaskEntry(void* arg);

  Config          _config;
  AudioSource*    _source = nullptr;
  AudioAnalyzer*  _analyzer = nullptr;
  DeviceManager   _devices;
  int             _resolvedSource = 1;   // source actually used this boot
  char            _sourceReason[20] = "configured";
  StripRuntime*   _strips[kMaxStrips] = {nullptr};
  int             _stripCount = 0;

  AudioFrame      _frameStorage;
  SemaphoreHandle_t _mutex = nullptr;
  uint32_t        _framesCount = 0;
  uint32_t        _lastSeenFrame = 0;
  uint32_t        _pendingSaveAt = 0;
  bool            _pendingSave = false;

  ArtNetNode*     _artnet = nullptr;
  SyncNode*       _sync = nullptr;
  WebUi*          _web = nullptr;

  cine::CinematicEngine _cinEngine;        // fusion core (pure, task-safe)
  cine::SceneAnalyzer   _cinAnalyzer;      // local audio -> cinematic features
  cine::Status          _cinStatus;
  SceneLinkNode*        _sceneLink = nullptr;  // companion SceneFrame receiver

  sys::SystemMode _sysMode;
  TaskHandle_t    _shutdownTask = nullptr;

  TaskHandle_t    _audioTask = nullptr;
  TaskHandle_t    _ledTask = nullptr;
};