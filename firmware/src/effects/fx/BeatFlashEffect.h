#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"

class BeatFlashEffect : public Effect {
public:
  const char* name() const override { return "Beat Flash"; }

  void begin(LedFrame&, const EffectParams&) override {
    _flash = 0.0f;
    _lastT = 0;
  }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    uint32_t t = a.timeMs;
    float dt = (_lastT == 0) ? 0.0f : (float)(t - _lastT) * 0.001f;
    _lastT = t;
    if (a.beat) {
      _flash = clamp01(_flash + 0.55f + 0.45f * a.beatStrength);
    } else {
      _flash -= dt * 3.5f;
    }
    if (_flash < 0.0f) _flash = 0.0f;
    float hue = effHueOffset(p, t * 0.001f);
    Rgb c = effRgb(p, hue, 0.25f, 0.62f, _flash);
    frame.fill(c);
  }

private:
  float    _flash = 0.0f;
  uint32_t _lastT = 0;
};