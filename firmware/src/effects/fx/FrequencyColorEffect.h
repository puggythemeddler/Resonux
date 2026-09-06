#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"

class FrequencyColorEffect : public Effect {
public:
  const char* name() const override { return "Frequency To Color"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    float tot = a.bass + a.lowMid + a.mid + a.highMid + a.treble;
    if (tot < 1e-3f) tot = 1e-3f;
    float h = (a.treble * 0.62f + a.highMid * 0.48f + a.mid * 0.30f +
               a.lowMid * 0.15f + a.bass * 0.06f) /
              tot;
    h += p.startHue / 360.0f;
    float v = effLevel(p, a.bass * 0.5f + a.amplitude * 0.7f);
    Rgb c = effRgb(p, h, (float)p.saturation / 255.0f, 0.5f, v);
    int n = frame.size();
    for (int i = 0; i < n; ++i) frame.set(i, c);
  }
};