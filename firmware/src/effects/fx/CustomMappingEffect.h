#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"

class CustomMappingEffect : public Effect {
public:
  const char* name() const override { return "Custom Mapping"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    int count = (p.bandCount > 0) ? p.bandCount : a.bandCount;
    if (count < 1) count = 1;
    float t = a.timeMs * 0.001f;
    float base = p.startHue / 360.0f;
    float rot = fmodf(p.hueSpeed * t, 360.0f) / 360.0f;
    for (int i = 0; i < n; ++i) {
      int band;
      if (p.zoneBand && i < p.zoneCount) {
        band = p.zoneBand[i];
      } else {
        band = (int)(((float)i / (float)(n > 1 ? n - 1 : 1)) * count);
      }
      if (band < 0) band = 0;
      if (band >= count) band = count - 1;
      float lvl = effLevel(p, a.bands[band]);
      float hue = base + rot + (float)band / (float)count * 0.55f +
                  lvl * 0.18f;
      frame.set(i, effRgb(p, hue, 0.9f, 0.5f, lvl));
    }
  }
};