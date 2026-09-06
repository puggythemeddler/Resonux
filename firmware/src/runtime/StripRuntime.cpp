#include "runtime/StripRuntime.h"
#include "effects/EffectRegistry.h"
#include "led/factory/LEDDriverFactory.h"
#include <string.h>

StripRuntime::~StripRuntime() {
  delete _driver;
  delete _effect;
  delete _frame;
}

bool StripRuntime::begin() {
  _driver = createLEDDriver(_cfg);
  if (!_driver || !_driver->begin()) return false;
  _effect = fx::create(_cfg.effectId);
  if (!_effect) return false;
  int n = _driver->segmentCount();
  if (n < 1) n = 1;
  _frame = new LedFrame(n);
  rebuildParams();
  _effect->begin(*_frame, _params);
  _driver->setMasterBrightness(_cfg.maxBrightness);
  _driver->clear();
  _driver->show();
  return true;
}

void StripRuntime::rebuildParams() {
  _params.maxBrightness = _cfg.maxBrightness;
  _params.minBrightness = _cfg.minBrightness;
  _params.sensitivity = _cfg.sensitivity;
  _params.startHue = _cfg.startHue;
  _params.hueSpeed = _cfg.hueSpeed;
  _params.decay = _cfg.decay;
  _params.palette = _cfg.palette;
  _params.zoneCount = _cfg.zoneCount;
  _params.bandCount = 0;
  if (_cfg.zoneCount > 0 && _cfg.zoneCount <= kMaxStripZones) {
    _params.zoneBand = &_cfg.zones[0].band;
  } else {
    _params.zoneBand = nullptr;
  }
}

void StripRuntime::step(const AudioFrame& audio, uint32_t nowMs) {
  if (!_frame || !_driver || !_effect) return;
  _effect->render(*_frame, audio, _params);
  for (int i = 0; i < _frame->size(); ++i) {
    const Rgb& c = _frame->at(i);
    _driver->setPixel(i, c.r, c.g, c.b);
  }
  _driver->show();
  _lastStepMs = nowMs;
}

const char* StripRuntime::driverName() const {
  return _driver ? _driver->driverName() : "none";
}