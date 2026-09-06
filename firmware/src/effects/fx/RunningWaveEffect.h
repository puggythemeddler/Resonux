#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class RunningWaveEffect : public Effect {
public:
  const char* name() const override { return "Running Wave"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    float t = a.timeMs * 0.001f;
    float base = p.startHue / 360.0f;
    float v = effLevel(p, 0.45f + 0.55f * a.amplitude);
    if (a.beat) v = clamp01(v + a.beatStrength * 0.30f);
    for (int i = 0; i < n; ++i) {
      float pos = (float)i / (float)n;
      float ph = pos * 2.0f - fmodf(t * (0.30f + 0.25f * a.amplitude), 2.0f);
      float hue = base + ph * 0.18f;
      frame.set(i, effRgb(p, hue, 0.95f, 0.5f, v));
    }
  }
};