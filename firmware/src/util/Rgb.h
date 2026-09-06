#pragma once
#include <stdint.h>

struct Rgb {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
};

inline uint8_t clamp255(float v) {
  if (v < 0.0f) return 0;
  if (v > 255.0f) return 255;
  return (uint8_t)v;
}

inline Rgb rgbf(float r, float g, float b) {
  Rgb c;
  c.r = clamp255(r);
  c.g = clamp255(g);
  c.b = clamp255(b);
  return c;
}

inline Rgb scale(const Rgb& c, float f) {
  return rgbf(c.r * f, c.g * f, c.b * f);
}

inline Rgb blend(const Rgb& a, const Rgb& b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return rgbf(a.r + (b.r - a.r) * t,
              a.g + (b.g - a.g) * t,
              a.b + (b.b - a.b) * t);
}

inline float luma(const Rgb& c) {
  return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

inline float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

inline Rgb hslToRgb(float h, float s, float l) {
  h = h - (float)(int)h;
  if (h < 0.0f) h += 1.0f;
  s = clamp01(s);
  l = clamp01(l);
  if (s <= 0.0f) {
    uint8_t v = (uint8_t)(l * 255.0f);
    Rgb c = {v, v, v};
    return c;
  }
  float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
  float p = 2.0f * l - q;
  auto hue = [&](float t) -> float {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * 6.0f * (2.0f / 3.0f - t);
    return p;
  };
  Rgb c;
  c.r = (uint8_t)(hue(h + 1.0f / 3.0f) * 255.0f);
  c.g = (uint8_t)(hue(h) * 255.0f);
  c.b = (uint8_t)(hue(h - 1.0f / 3.0f) * 255.0f);
  return c;
}

enum PaletteId {
  PALETTE_HEAT = 0,
  PALETTE_RAINBOW,
  PALETTE_OCEAN,
  PALETTE_PARTY,
  PALETTE_REDBLUE,
  PALETTE_COUNT
};

inline Rgb paletteColor(int id, float t) {
  t = clamp01(t);
  switch (id) {
    case PALETTE_RAINBOW:
      return hslToRgb(t, 1.0f, 0.5f);
    case PALETTE_OCEAN:
      return hslToRgb(0.52f + 0.14f * t, 0.85f, 0.22f + 0.45f * t);
    case PALETTE_PARTY:
      return hslToRgb(0.65f - 0.5f * t, 1.0f, 0.5f);
    case PALETTE_REDBLUE:
      return blend(Rgb{255, 0, 0}, Rgb{0, 40, 255}, t);
    case PALETTE_HEAT:
    default: {
      if (t < 0.25f) return scale(Rgb{255, 0, 0}, t * 4.0f);
      if (t < 0.5f) return rgbf(255, 0, (t - 0.25f) * 4.0f * 255.0f);
      if (t < 0.75f) return Rgb{255, (uint8_t)((t - 0.5f) * 4.0f * 255.0f), 255};
      return Rgb{255, 255, 255};
    }
  }
}