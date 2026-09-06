#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class VuMeterEffect : public Effect {
public:
  const char* name() const override { return "VU Meter"; }

  void begin(LedFrame&, const EffectParams&) override { _peak = 0.0f; }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    if (n == 0) return;
    float lev = effLevel(p, a.amplitude);
    float t = a.timeMs * 0.001f;
    if (lev >= _peak) _peak = lev;
    else _peak = _peak * powf(0.86f, t - _lastT);
    _lastT = t;

    float half = (float)(n - 1) * 0.5f;
    for (int i = 0; i < n; ++i) {
      float d = (n > 1) ? fabsf((float)i - half) / half : 1.0f;
      float lit = 1.0f - d;
      float colT = 1.0f - lit;
      Rgb c = Rgb{0, 0, 0};
      if (lit <= lev) {
        c = paletteColor(p.palette, 1.0f - lit);
        float b = (float)p.minBrightness +
                  (float)(p.maxBrightness - p.minBrightness) * (1.0f - d);
        c = scale(c, b / 255.0f);
      }
      int j = i;
      if (d < 0.04f && lev > 0.02f) {
        c = scale(Rgb{255, 255, 255}, p.maxBrightness / 255.0f * 0.9f);
      }
      frame.set(j, c);
    }
  }

private:
  float    _peak = 0.0f;
  float    _lastT = 0.0f;
};