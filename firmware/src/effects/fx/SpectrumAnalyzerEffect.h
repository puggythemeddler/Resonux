#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"

class SpectrumAnalyzerEffect : public Effect {
public:
  const char* name() const override { return "Spectrum Analyzer"; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    int count = (p.bandCount > 0) ? p.bandCount : a.bandCount;
    if (count < 1) count = 1;
    int barWidth = (n + count - 1) / count;
    if (barWidth < 1) barWidth = 1;
    for (int i = 0; i < n; ++i) {
      float prog = (float)i / (float)n;
      int band = (int)(prog * count);
      if (band >= count) band = count - 1;
      int within = i - band * barWidth;
      float frac = (float)within / (float)barWidth;
      float level = clamp01(a.bands[band]);
      Rgb c = Rgb{0, 0, 0};
      if (frac <= level * 0.9f || (level <= 0.01f && within == 0)) {
        c = themeColor(p, level);
        float b = (float)p.minBrightness +
                  (float)(p.maxBrightness - p.minBrightness) * level * 0.55f;
        b *= p.themeBright;
        c = scale(c, b / 255.0f);
      } else if (frac >= level * 0.9f && frac <= level * 0.9f + 0.08f) {
        c = scale(Rgb{255, 255, 255}, p.maxBrightness / 255.0f * 0.9f);
      }
      frame.set(i, c);
    }
  }
};