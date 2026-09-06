#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class EnergyPulseEffect : public Effect {
public:
  const char* name() const override { return "Energy Pulse"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    float t = a.timeMs * 0.001f;
    float e = effLevel(p, 0.5f * a.amplitude + 0.5f * a.bass);
    float ph = fmodf(t * 0.55f, 1.0f);
    float base = p.startHue / 360.0f;
    float rot = fmodf(p.hueSpeed * t, 360.0f) / 360.0f;
    for (int i = 0; i < n; ++i) {
      float pos = (float)i / (float)(n - 1);
      float dist = fabsf(pos * 2.0f - 1.0f);
      float ring = fabsf(dist - (1.0f - ph));
      float bv = e * expf(-ring * ring * 55.0f);
      float hv = base + rot + ph * 0.5f;
      frame.set(i, effRgb(p, hv, 0.85f, 0.5f, bv));
    }
  }
};