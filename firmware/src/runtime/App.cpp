#include "runtime/App.h"
#include "audio/I2SMicSource.h"
#include "audio/SourceSelector.h"
#include "audio/TestToneSource.h"
#include "config/ConfigDefaults.h"
#include "config/ConfigStore.h"
#include "effects/EffectRegistry.h"
#include "sync/SyncNode.h"
#include "theme/ThemeEngine.h"
#include "ui/TouchUi.h"
#include "util/Log.h"
#include "web/WebUi.h"
#include <Arduino.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

App& App::instance() {
  static App app;
  return app;
}

bool App::begin() {
  Serial.begin(115200);
  delay(200);
  logBoot("boot", "Universal Music-Reactive LED Controller");

  ConfigStore::begin();
  bool loaded = ConfigStore::load(_config);
  logBoot("cfg", "config %s", loaded ? "loaded" : "defaults applied");
  if (!loaded) ConfigStore::save(_config);

  ThemeEngine::instance().begin(_config);
  logBoot("theme", "engine ready: %d themes, active=%s",
          ThemeEngine::instance().count(), ThemeEngine::instance().activeId());

  DisplayManager& disp = DisplayManager::instance();
  TouchUi::instance().begin(disp);  // builds drivers + mgr.begin() when enabled
  if (disp.enabled()) {
    disp.setDrawCallback(TouchUi::renderStatic, nullptr);
    logBoot("display", "panel up: %dx%d touch=%s timeout=%ds",
            disp.panelW(), disp.panelH(),
            _config.display.touch == TOUCH_NONE ? "none" : "yes",
            _config.display.screenTimeoutS);
  } else {
    logBoot("display", "panel disabled");
  }

  WifiManager& wifi = WifiManager::instance();
  if (wifi.begin(_config.net)) {
    logBoot("wifi", "mode=%s ip=%s", wifi.modeName(),
            wifi.ip().toString().c_str());
  } else {
    logBoot("wifi", "disabled (%s)", wifi.lastError());
  }

  _devices.begin(_config);  // needs the radio up to bind the responder
  logBoot("device", "manager: %d rows, responder=%d", _devices.count(),
          _devices.responderUp());

  _web = new WebUi();
  if (_web->begin(*this)) {
    logBoot("web", "dashboard + OTA up");
  } else {
    logBoot("web", "init FAILED");
    delete _web;
    _web = nullptr;
  }

  const bool slaveRole =
      _config.sync.enabled && _config.sync.role == SYNC_SLAVE;

  // Boot-time audio source: honour the persisted source, or let the
  // hysteresis-aware SourceSelector pick preferred-vs-fallback when auto is
  // enabled. Live switching is intentionally deferred — the analyzer pipeline
  // is built once here, so a source change is applied cleanly on the next boot.
  const bool micPresent = !slaveRole && _config.micData >= 0;
  int bootSource = _config.audioSource;
  if (_config.autoSelectSource) {
    dev::SourceSelector sel;
    sel.configure(true, (SourceKind)_config.preferredSource,
                  (SourceKind)_config.fallbackSource, (SourceKind)_config.audioSource);
    uint32_t mask = dev::sourceMaskOf(SOURCE_TEST);
    if (micPresent) mask |= dev::sourceMaskOf(SOURCE_MIC);
    bootSource = (int)sel.update(0, mask);
    logBoot("audio", "auto-select: source=%s", sourceKindIdent((SourceKind)bootSource));
  }
  _resolvedSource = bootSource;
  if (slaveRole) {
    strncpy(_sourceReason, "sync_slave", sizeof(_sourceReason) - 1);
  } else if (bootSource == SOURCE_TEST) {
    strncpy(_sourceReason, "test_tone", sizeof(_sourceReason) - 1);
  } else if (_config.autoSelectSource) {
    if (bootSource == _config.preferredSource) {
      strncpy(_sourceReason, "auto_preferred", sizeof(_sourceReason) - 1);
    } else if (bootSource == _config.fallbackSource) {
      strncpy(_sourceReason, "auto_fallback", sizeof(_sourceReason) - 1);
    } else {
      strncpy(_sourceReason, "auto_none", sizeof(_sourceReason) - 1);
    }
  } else {
    strncpy(_sourceReason, "configured", sizeof(_sourceReason) - 1);
  }

  const bool testTone = bootSource == SOURCE_TEST;
  if (slaveRole) {
    logBoot("audio", "mic skipped (sync slave - frames arrive over network)");
  } else if (testTone) {
    _source = new TestToneSource(_config.audio.sampleRate);
    logBoot("audio", "test-tone source active @ %.0f Hz",
            _config.audio.sampleRate);
    _analyzer = new AudioAnalyzer(_config.audio, _source);
    if (!_analyzer->begin()) {
      logBoot("audio", "analyzer init FAILED");
      return false;
    }
  } else if (bootSource == SOURCE_NONE) {
    logBoot("audio", "no input selected (source=none) - lights idle");
  } else {
    MicPins mic = {_config.micSck, _config.micWs, _config.micData};
    _source = new I2SMicSource(mic, _config.audio.sampleRate);
    if (!_source || !_source->begin()) {
      logBoot("audio", "I2S microphone init FAILED");
    } else {
      logBoot("audio", "mic ready: %s @ %.0f Hz", _source->name(),
              _source->sampleRate());
    }

    _analyzer = new AudioAnalyzer(_config.audio, _source);
    if (!_analyzer->begin()) {
      logBoot("audio", "analyzer init FAILED");
      return false;
    }
  }

  buildStrips();

  if (_config.artnet.enabled) {
    _artnet = new ArtNetNode(_config.artnet, _config.fixtures,
                             _config.fixtureCount);
    if (_artnet->begin()) {
      logBoot("artnet", "node active (%s)", _artnet->statusString());
    } else {
      logBoot("artnet", "init FAILED or disabled");
      delete _artnet;
      _artnet = nullptr;
    }
  }

  _mutex = xSemaphoreCreateMutex();
  if (!_mutex) return false;

  if (_config.sync.enabled && _config.sync.role != SYNC_OFF) {
    _sync = new SyncNode();
    if (_sync->begin(
            _config.sync, [this](const AudioFrame& f) { injectRemoteFrame(f); })) {
      logBoot("sync", "node active (%s)", _sync->roleName());
    } else {
      logBoot("sync", "init FAILED or disabled");
      delete _sync;
      _sync = nullptr;
    }
  }

  xTaskCreatePinnedToCore(App::audioTaskEntry, "audio", 4096, this, 24,
                          &_audioTask, 1);
  xTaskCreatePinnedToCore(App::ledTaskEntry, "led", 4096, this, 20, &_ledTask,
                          0);
  logBoot("boot", "system ready - strips=%d", _stripCount);
  return true;
}

void App::buildStrips() {
  _stripCount = _config.stripCount;
  if (_stripCount > kMaxStrips) _stripCount = kMaxStrips;
  for (int i = 0; i < _stripCount; ++i) {
    StripRuntime* st = new StripRuntime(_config.strips[i]);
    if (st->begin()) {
      _strips[i] = st;
      logBoot("strip", "[%d] %s driver=%s effect=%s segs=%d", i,
              _config.strips[i].name, st->driverName(),
              fx::nameOf(_config.strips[i].effectId), st->segmentCount());
    } else {
      delete st;
      _strips[i] = nullptr;
      logBoot("strip", "[%d] init FAILED", i);
    }
  }
}

bool App::takeFrame(AudioFrame& out) {
  if (!_mutex) return false;
  if (!xSemaphoreTake(_mutex, 0)) return false;
  out = _frameStorage;
  xSemaphoreGive(_mutex);
  return true;
}

void App::injectRemoteFrame(const AudioFrame& f) {
  if (!_mutex) return;
  if (!xSemaphoreTake(_mutex, pdMS_TO_TICKS(5))) return;
  _frameStorage = f;
  ++_framesCount;
  xSemaphoreGive(_mutex);
}

bool App::setMasterBrightness(uint8_t value) {
  _config.masterBrightness = value;
  return ConfigStore::save(_config);
}

bool App::setStripEffect(int strip, int effectId) {
  if (strip < 0 || strip >= _stripCount) return false;
  if (effectId < 0 || effectId >= EFFECT_COUNT) return false;
  StripRuntime* st = _strips[strip];
  if (!st || !st->setEffect(effectId)) return false;
  _config.strips[strip].effectId = effectId;
  return ConfigStore::save(_config);
}

bool App::setTheme(const char* id) {
  if (!id || !ThemeEngine::instance().apply(id)) return false;
  strncpy(_config.themeId, id, sizeof(_config.themeId) - 1);
  return ConfigStore::save(_config);
}

bool App::setAudioSource(int kind) {
  if (kind < SOURCE_NONE || kind >= SOURCE_COUNT) return false;
  _config.audioSource = kind;
  return ConfigStore::save(_config);
}

bool App::setPreferredSource(int kind) {
  if (kind < SOURCE_NONE || kind >= SOURCE_COUNT) return false;
  _config.preferredSource = kind;
  return ConfigStore::save(_config);
}

bool App::setAutoSelect(bool on) {
  _config.autoSelectSource = on;
  return ConfigStore::save(_config);
}

bool App::setSensitivity(float value) {
  if (value < 0.0f || value > 5.0f) return false;
  for (int i = 0; i < _stripCount; ++i) {
    StripRuntime* st = _strips[i];
    if (!st) continue;
    _config.strips[i].sensitivity = value;
    st->refreshParams();
  }
  return ConfigStore::save(_config);
}

bool App::setDisplayBacklightPct(int pct) {
  if (pct < 0 || pct > 100) return false;
  _config.display.backlightPct = pct;
  DisplayManager::instance().setBacklightPct((uint8_t)pct);
  return ConfigStore::save(_config);
}

bool App::setDisplayTimeout(int seconds) {
  if (seconds < 0 || seconds > 86400) return false;
  _config.display.screenTimeoutS = seconds;
  return ConfigStore::save(_config);
}

bool App::requestRestart() { return startShutdown(sys::Action::Restart); }
bool App::requestPowerOff() { return startShutdown(sys::Action::PowerOff); }

bool App::startShutdown(sys::Action a) {
  if (!_sysMode.request(a)) return false;  // already shutting down
  xTaskCreatePinnedToCore(App::shutdownTaskEntry, "shutdown", 4096, this, 12,
                          &_shutdownTask, 0);
  return true;
}

void App::extinguishLeds() {
  for (int i = 0; i < _stripCount; ++i) {
    StripRuntime* st = _strips[i];
    if (!st) continue;
    LEDDriver* d = st->driver();
    if (!d) continue;
    d->clear();
    d->show();
  }
}

void App::gracefulShutdown() {
  sys::SystemMode& m = _sysMode;
  m.beginShutdown();
  logBoot("sys", "shutting down (%s)", m.actionName());

  // 1. Quiet real-time outputs: strips, DMX, sync.
  extinguishLeds();
  if (_artnet) {
    _artnet->blackout();
    _artnet->stop();
  }
  if (_sync) _sync->stop();

  // 2. Persist whatever state matters, best-effort and bounded.
  ConfigStore::save(_config);

  // 3. Last chance for the web client to see the ack / status change.
  vTaskDelay(pdMS_TO_TICKS(500));

  m.complete();

  if (m.pending() == sys::Action::PowerOff) {
    logBoot("sys", "entering low-power state");
    DisplayManager::instance().sleep();  // backlight off before sleep
    const int wakePin = _config.display.wakePin;
    if (wakePin >= 0 && wakePin < 48) {
      esp_sleep_enable_ext0_wakeup((gpio_num_t)wakePin, 1);
      logBoot("sys", "wake on GPIO%d rising edge armed", wakePin);
    } else {
      logBoot("sys", "no wakePin configured - use reset/power to wake");
    }
    esp_deep_sleep_start();  // noreturn until wake / reset
  } else {
    logBoot("sys", "restarting");
    ESP.restart();
  }
}

void App::shutdownTaskEntry(void* arg) {
  App* app = (App*)arg;
  app->gracefulShutdown();
  vTaskSuspend(nullptr);  // unreachable after restart/sleep
}

void App::audioLoop() {
  // Slaves render from the network-multicast frame; the mic pipeline idles.
  if (_sync && _sync->role() == SYNC_SLAVE) {
    vTaskDelay(20);
    return;
  }
  _analyzer->process(millis());
  uint32_t fc = _analyzer->frameCount();
  if (fc != _lastSeenFrame) {
    AudioFrame f;
    _analyzer->getFrame(f);
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5))) {
      _frameStorage = f;
      _framesCount = fc;
      xSemaphoreGive(_mutex);
    }
    if (_sync) _sync->publishFrame(f);
    if (_artnet) _artnet->setAudioFrame(f);
    _lastSeenFrame = fc;
  }
  vTaskDelay(1);
}

void App::ledLoop() {
  if (_sysMode.shuttingDown()) {
    vTaskDelay(20);  // outputs are quiesced; stop rendering during shutdown
    return;
  }
  AudioFrame f;
  takeFrame(f);
  uint32_t now = millis();
  _devices.tick(now);

  DisplayManager& disp = DisplayManager::instance();
  TouchUi& ui = TouchUi::instance();
  ui.tick(now);
  static uint32_t lastTouchPoll = 0;
  if (now - lastTouchPoll >= 30) {
    lastTouchPoll = now;
    TouchPoint tp;
    if (disp.pollTouch(tp)) ui.handleTouch(tp);
  }
  ui.updateAudio(f);
  disp.update(now);

  const float master =
      _config.masterBrightness ? _config.masterBrightness / 255.0f : 1.0f;
  ThemeEngine& te = ThemeEngine::instance();
  for (int i = 0; i < _stripCount; ++i) {
    StripRuntime* st = _strips[i];
    if (!st) continue;
    uint32_t period = 1000u / (st->cfg().targetFps ? st->cfg().targetFps : 60);
    if (now - st->lastStepMs() >= period) {
      Themes::ThemeFrame th = te.processStrip(i, f, now);
      th.brightness *= master;
      st->step(f, now, &th);
    }
  }
  static uint32_t lastDiag = 0;
  if (now - lastDiag >= 3000) {
    lastDiag = now;
    Serial.printf(
        "[diag] fps=%.0f amp=%.2f bass=%.2f mid=%.2f treble=%.2f beat=%d %.2f heap=%u",
        _analyzer ? _analyzer->fps() : 0.0f, f.amplitude, f.bass, f.mid,
        f.treble, f.beat ? 1 : 0, f.beatStrength, (unsigned)ESP.getFreeHeap());
    if (_sync) {
      Serial.printf(" sync=%s", _sync->roleName());
      if (_sync->role() == SYNC_SLAVE) {
        Serial.printf(" seq=%u off=%ldms alive=%d", _sync->seq(),
                      (long)_sync->clockOffsetMs(), _sync->masterAlive() ? 1 : 0);
      }
    }
    Serial.println();
  }
  vTaskDelay(1);
}

void App::audioTaskEntry(void* arg) {
  App* app = (App*)arg;
  for (;;) app->audioLoop();
}

void App::ledTaskEntry(void* arg) {
  App* app = (App*)arg;
  for (;;) app->ledLoop();
}