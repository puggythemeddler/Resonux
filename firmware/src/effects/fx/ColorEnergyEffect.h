#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"

class ColorEnergyEffect : public Effect {
public:
  const char* name() const override { return "Color Energy"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    float tot = a.bass + a.lowMid + a.mid + a.highMid + a.treble;
    if (tot < 1e-3f) tot = 1e-3f;
    float h = (a.bass * 0.02f + a.lowMid * 0.12f + a.mid * 0.28f +
               a.highMid * 0.44f + a.treble * 0.60f) /
              tot;
    h += p.startHue / 360.0f;
    float v = effLevel(p, 0.35f * a.amplitude + 0.50f * a.bass +
                          0.15f * a.mid);
    if (a.beat) v = clamp01(v + a.beatStrength * 0.30f);
    Rgb c = effRgb(p, h, (float)p.saturation / 255.0f, 0.5f, v);
    for (int i = 0; i < n; ++i) frame.set(i, c);
  }
};