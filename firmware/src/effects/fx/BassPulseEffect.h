#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class BassPulseEffect : public Effect {
public:
  const char* name() const override { return "Bass Pulse"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    float t = a.timeMs * 0.001f;
    float hue = effHueOffset(p, t);
    float v = effLevel(p, powf(a.bass, 1.35f));
    v += a.beat ? a.beatStrength * 0.35f : 0.0f;
    Rgb c = effRgb(p, hue, 0.85f, 0.5f, v);
    frame.fill(c);
  }
};