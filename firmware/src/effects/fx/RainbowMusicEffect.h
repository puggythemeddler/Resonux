#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class RainbowMusicEffect : public Effect {
public:
  const char* name() const override { return "Rainbow Music"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    float t = a.timeMs * 0.001f;
    float base = p.startHue / 360.0f;
    float rot = fmodf(p.hueSpeed * t, 360.0f) / 360.0f;
    float v = effLevel(p, 0.35f + 0.65f * a.amplitude);
    if (a.beat) v = clamp01(v + a.beatStrength * 0.40f);
    for (int i = 0; i < n; ++i) {
      float hv = base + rot + (float)i * 0.02f;
      frame.set(i, effRgb(p, hv, 0.9f, 0.5f, v));
    }
  }
};