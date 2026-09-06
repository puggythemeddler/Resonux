#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class MusicWaveEffect : public Effect {
public:
  const char* name() const override { return "Music Wave"; }

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
      float lvl = clamp01(a.bands[band] * 0.7f + a.amplitude * 0.3f);
      float w = sinf(2.0f * kPi * (prog * 4.0f - t * (1.4f + 0.8f * a.amplitude)));
      float v = lvl * (0.55f + 0.45f * (0.5f + 0.5f * w));
      float hue = base + rot + prog * 0.45f + lvl * 0.18f;
      frame.set(i, effRgb(p, hue, 0.9f, 0.5f, effLevel(p, v)));
    }
  }
};