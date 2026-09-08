#include "runtime/App.h"
#include "audio/I2SMicSource.h"
#include "config/ConfigDefaults.h"
#include "config/ConfigStore.h"
#include "effects/EffectRegistry.h"
#include "theme/ThemeEngine.h"
#include "ui/TouchUi.h"
#include "util/Log.h"
#include "web/WebUi.h"
#include <Arduino.h>
#include <string.h>

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

  _web = new WebUi();
  if (_web->begin(*this)) {
    logBoot("web", "dashboard + OTA up");
  } else {
    logBoot("web", "init FAILED");
    delete _web;
    _web = nullptr;
  }

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

void App::audioLoop() {
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
    if (_artnet) _artnet->setAudioFrame(f);
    _lastSeenFrame = fc;
  }
  vTaskDelay(1);
}

void App::ledLoop() {
  AudioFrame f;
  takeFrame(f);
  uint32_t now = millis();

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
        "[diag] fps=%.0f amp=%.2f bass=%.2f mid=%.2f treble=%.2f beat=%d %.2f heap=%u\n",
        _analyzer ? _analyzer->fps() : 0.0f, f.amplitude, f.bass, f.mid,
        f.treble, f.beat ? 1 : 0, f.beatStrength, (unsigned)ESP.getFreeHeap());
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