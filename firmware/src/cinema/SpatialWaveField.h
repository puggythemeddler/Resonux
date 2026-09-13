#pragma once
#include <stdint.h>
#include <math.h>

// Spatial wave propagation for Cinematic room mapping. The SceneFrame carries
// only a focal point (focusX/focusY from the companion) plus an event
// amplitude; this field turns that pair into a per-strip response that moves
// across the room: centre-screen action lights the centre strips first, a
// panning shot sweeps the light across the wall, an explosion blooms outward
// from its origin.
//
// Wave response per active wave (all cheap, no transcendentals):
//   front = speed * age                      metres the wavefront has travelled
//   bell  = 1 / (1 + k * (dist - front)^2)   gaussian-shaped wavefront bulge
//   fade  = 1 / (1 + decay * age)            energy bleeding out over time
//   energy += amplitude * bell * fade
// A point wave radiates from its origin; a line wave advances along a unit
// ray (used for scene cuts / chases that pan left-to-right).
//
// Coordinates are normalised 0..1 (x: left->right, y: top->bottom) to match
// focusX/focusY/255; a strip placed at (0.5, 0.5) sees a centre focus at full
// strength. Waves live in a fixed ring of maxWaves slots (4..12) so RAM is
// bounded and a new hit always displaces the oldest wave.
//
// intensityAt() returns 0 when no wave reaches the point (rest) and appraches
// 1 near a fresh, strong wave. The caller maps that to a brightness
// multiplier (the apply layer keeps a quiet floor so rest is not black).
//
// Pure C++ for host tests: time is passed in so the clock is deterministic.
namespace cine {

class SpatialWaveField {
 public:
  static constexpr int kMaxWaves = 12;
  // hard cap on a wave's useful life (the fade factor has collapsed by then)
  static constexpr float kMaxWaveAgeS = 3.0f;

  struct Wave {
    float x = 0.5f, y = 0.5f;
    float dx = 0.0f, dy = 0.0f;   // unit ray direction when line == true
    float amplitude = 0.0f;       // 0..1
    float speed = 2.0f;           // room units per second
    float decay = 0.8f;           // per-second fade coefficient
    float widthK = 2.0f;          // 1 / width^2 (precomputed bell curvature)
    uint32_t birthMs = 0;
    bool line = false;
    bool active = false;
  };

  SpatialWaveField() = default;

  void reset() {
    for (int i = 0; i < kMaxWaves; ++i) {
      _w[i].active = false;
    }
    _head = 0;
  }

  // Ring depth (4..12); shrinking leaves older slots dormant until overwritten.
  void setMaxWaves(int n) {
    if (n < 4) n = 4;
    if (n > kMaxWaves) n = kMaxWaves;
    _maxWaves = n;
    if (_head >= _maxWaves) _head = 0;
  }
  int maxWaves() const { return _maxWaves; }

  // A point wave: light radiates outward from (x, y).
  void spawn(float x, float y, float amplitude, float speed, float decay,
             float width, uint32_t nowMs) {
    _put(x, y, 0.0f, 0.0f, false, amplitude, speed, decay, width, nowMs);
  }

  // A line wave: the wavefront advances along the ray (dx, dy) from the
  // origin. |projection onto the ray| is the distance used, so a wall along
  // the pan direction lights in order instead of all at once.
  void spawnLine(float x, float y, float dx, float dy, float amplitude,
                 float speed, float decay, float width, uint32_t nowMs) {
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) {
      _put(x, y, 0.0f, 0.0f, false, amplitude, speed, decay, width, nowMs);
      return;
    }
    _put(x, y, dx / len, dy / len, true, amplitude, speed, decay, width,
         nowMs);
  }

  int activeCount() const {
    int n = 0;
    for (int i = 0; i < _maxWaves; ++i) {
      if (_w[i].active) ++n;
    }
    return n;
  }

  // Normalised wave energy at a room point at the given time, clamped to
  // 0..1. Deterministic in (x, y, nowMs).
  float intensityAt(float x, float y, uint32_t nowMs) const {
    float energy = 0.0f;
    for (int i = 0; i < _maxWaves; ++i) {
      const Wave& w = _w[i];
      if (!w.active) continue;
      const uint32_t ageMs = nowMs >= w.birthMs ? nowMs - w.birthMs : 0u;
      const float ageS = (float)ageMs * (1.0f / 1000.0f);
      if (ageS > kMaxWaveAgeS) continue;
      float d;
      if (w.line) {
        // distance along the ray direction (absolute: behind the front is lit
        // already, ahead gets lit when the front arrives)
        const float px = x - w.x;
        const float py = y - w.y;
        d = fabsf(px * w.dx + py * w.dy);
      } else {
        const float px = x - w.x;
        const float py = y - w.y;
        d = sqrtf(px * px + py * py);
      }
      const float front = w.speed * ageS;
      const float r = d - front;
      const float bell = 1.0f / (1.0f + r * r * w.widthK);
      const float fade = 1.0f / (1.0f + w.decay * ageS);
      energy += w.amplitude * bell * fade;
    }
    if (energy > 1.0f) energy = 1.0f;
    return energy;
  }

 private:
  void _put(float x, float y, float dx, float dy, bool line, float amplitude,
            float speed, float decay, float width, uint32_t nowMs) {
    Wave& w = _w[_head];
    w.x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    w.y = y < 0.0f ? 0.0f : (y > 1.0f ? 1.0f : y);
    w.dx = dx;
    w.dy = dy;
    w.line = line;
    w.amplitude = amplitude < 0.0f ? 0.0f : (amplitude > 1.0f ? 1.0f : amplitude);
    w.speed = speed < 0.05f ? 0.05f : speed;
    w.decay = decay < 0.05f ? 0.05f : decay;
    const float wd = width < 0.05f ? 0.05f : width;  // keep the bell finite
    w.widthK = 1.0f / (wd * wd);
    w.birthMs = nowMs;
    w.active = true;
    _head = (_head + 1) % _maxWaves;
  }

  Wave _w[kMaxWaves];
  int _head = 0;
  int _maxWaves = kMaxWaves;
};

}  // namespace cine