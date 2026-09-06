#pragma once
#include "effects/Effect.h"
#include "effects/EffectUtil.h"
#include <math.h>

class BeatRippleEffect : public Effect {
public:
  const char* name() const override { return "Beat Ripple"; }

  struct Ring {
    float pos = -1.0f;
    float energy = 0.0f;
    float speed = 1.0f;
  };

  void begin(LedFrame&, const EffectParams&) override {
    for (int i = 0; i < kMaxRings; ++i) {
      _rings[i].pos = -1.0f;
      _rings[i].energy = 0.0f;
    }
    _active = 0;
    _lastT = 0;
  }

  void render(LedFrame& frame, const AudioFrame& a,
              const EffectParams& p) override {
    int n = frame.size();
    float t = a.timeMs * 0.001f;
    float dt = (_lastT == 0.0f) ? 0.016f : t - _lastT;
    _lastT = t;

    if (a.beat) {
      int r = _active;
      _rings[r].pos = 0.0f;
      _rings[r].energy = 0.5f + 0.5f * a.beatStrength;
      _rings[r].speed = 1.0f + 0.8f * a.beatStrength;
      _active = (_active + 1) % kMaxRings;
    }

    float base = p.startHue / 360.0f;
    float rot = fmodf(p.hueSpeed * t, 360.0f) / 360.0f;
    for (int i = 0; i < n; ++i) {
      float pos = (float)i / (float)(n > 1 ? n - 1 : 1);
      float bv = 0.0f;
      float hue = base + rot;
      for (int r = 0; r < kMaxRings; ++r) {
        Ring& ring = _rings[r];
        if (ring.pos < 0.0f) continue;
        float d = fabsf(pos - ring.pos);
        float g = ring.energy * expf(-d * d * 120.0f);
        if (g > bv) {
          bv = g;
          hue = base + rot + (float)r * 0.13f;
        }
      }
      frame.set(i, effRgb(p, hue, 0.9f, 0.5f, effLevel(p, bv)));
    }

    for (int r = 0; r < kMaxRings; ++r) {
      if (_rings[r].pos < 0.0f) continue;
      _rings[r].pos += _rings[r].speed * dt;
      _rings[r].energy *= powf(0.15f, dt);
      if (_rings[r].pos > 1.35f || _rings[r].energy < 0.01f) {
        _rings[r].pos = -1.0f;
      }
    }
  }

private:
  static const int kMaxRings = 5;
  Ring  _rings[kMaxRings];
  int   _active = 0;
  float _lastT = 0.0f;
};