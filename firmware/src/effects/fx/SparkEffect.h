#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <string.h>

class SparkEffect : public Effect {
public:
  const char* name() const override { return "High Frequency Spark"; }

  void begin(LedFrame& frame, const EffectParams&) override {
    _n = frame.size();
    if (_v) delete[] _v;
    _v = new float[_n];
    memset(_v, 0, sizeof(float) * _n);
    _seed = 0x2F6E2B1;
  }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    if (!_v) return;
    float treb = effLevel(p, a.treble * 0.8f + a.highMid * 0.3f);
    int nAdd = (int)(treb * _n * 0.6f);
    if (a.beat) nAdd += (int)(a.beatStrength * _n * 0.3f);
    for (int k = 0; k < nAdd; ++k) {
      _seed = _seed * 1664525u + 1013904223u;
      int i = (int)((_seed >> 16) % (uint32_t)_n);
      _v[i] += 0.30f + 0.70f * treb;
      if (_v[i] > 1.0f) _v[i] = 1.0f;
    }
    for (int i = 0; i < _n; ++i) {
      _v[i] *= 0.90f;
      frame.set(i, scale(paletteColor(p.palette, _v[i]),
                         (float)p.maxBrightness / 255.0f * _v[i]));
    }
  }

private:
  float*   _v = nullptr;
  int      _n = 0;
  uint32_t _seed = 0;
};