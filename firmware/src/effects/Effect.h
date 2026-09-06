#pragma once
#include "audio/AudioFrame.h"
#include "effects/EffectParams.h"
#include "effects/LedFrame.h"

enum EffectId {
  EFFECT_SPECTRUM = 0,
  EFFECT_BASS_PULSE,
  EFFECT_BEAT_FLASH,
  EFFECT_FREQ_WAVE,
  EFFECT_FREQUENCY_COLOR,
  EFFECT_RAINBOW,
  EFFECT_VU_METER,
  EFFECT_ENERGY_PULSE,
  EFFECT_RUNNING_WAVE,
  EFFECT_SPARK,
  EFFECT_GRADIENT,
  EFFECT_BEAT_RIPPLE,
  EFFECT_MUSIC_WAVE,
  EFFECT_COLOR_ENERGY,
  EFFECT_CUSTOM_MAPPING,
  EFFECT_COUNT
};

class Effect {
public:
  virtual ~Effect() {}

  virtual const char* name() const = 0;
  virtual void begin(LedFrame& frame, const EffectParams& p) {}
  virtual void render(LedFrame& frame, const AudioFrame& audio,
                      const EffectParams& p) = 0;
};