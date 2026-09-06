#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"

class GradientEffect : public Effect {
public:
  const char* name() const override { return "Bass To Treble Gradient"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    int count = (p.bandCount > 0) ? p.bandCount : a.bandCount;
    if (count < 1) count = 1;
    float t = a.timeMs * 0.001f;
    float base = p.startHue / 360.0f;
    float rot = fmodf(p.hueSpeed * t, 360.0f) / 360.0f;
    for (int i = 0; i < n; ++i) {
      float prog = (float)i / (float)n;
      int band = (int)(prog * count);
      if (band >= count) band = count - 1;
      float hue = base + rot + prog * 0.62f + a.bass * 0.08f +
                  a.highMid * 0.05f;
      float v = effLevel(p, a.bands[band] * 0.8f + a.amplitude * 0.2f);
      frame.set(i, effRgb(p, hue, 0.9f, 0.5f, v));
    }
  }
};