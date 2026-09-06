#pragma once
#include "audio/AudioFrame.h"
#include "config/Config.h"
#include "effects/Effect.h"
#include "effects/LedFrame.h"
#include "led/LEDDriver.h"

class StripRuntime {
public:
  explicit StripRuntime(const StripConfig& cfg) : _cfg(cfg) {}
  ~StripRuntime();

  bool begin();
  void step(const AudioFrame& audio, uint32_t nowMs);

  const LEDDriver* driver() const { return _driver; }
  LEDDriver* driver() { return _driver; }
  const Effect* effect() const { return _effect; }
  const char* driverName() const;
  int segmentCount() const { return _frame ? _frame->size() : 0; }
  uint32_t lastStepMs() const { return _lastStepMs; }
  const StripConfig& cfg() const { return _cfg; }

private:
  void rebuildParams();

  const StripConfig& _cfg;
  LEDDriver*   _driver = nullptr;
  Effect*      _effect = nullptr;
  LedFrame*    _frame = nullptr;
  EffectParams _params;
  uint32_t     _lastStepMs = 0;
  bool         _paramsBuilt = false;
};