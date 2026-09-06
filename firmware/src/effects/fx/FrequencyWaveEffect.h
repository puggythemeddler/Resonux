#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class FrequencyWaveEffect : public Effect {
public:
  const char* name() const override { return "Frequency Wave"; }

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
      float hv = base + rot + prog * 0.5f + a.amplitude * 0.25f +
                 a.bands[band] * 0.3f;
      float v = effLevel(p, a.bands[band] * 0.7f + a.amplitude * 0.3f);
      frame.set(i, effRgb(p, hv, 0.9f, 0.5f, v));
    }
  }
};