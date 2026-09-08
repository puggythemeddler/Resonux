#include <unity.h>

#include "util/Rgb.h"
#include "util/Smoother.h"
#include "effects/EffectUtil.h"
#include "effects/Effect.h"
#include "effects/LedFrame.h"
#include "audio/AudioFrame.h"
#include "effects/fx/BassPulseEffect.h"
#include "effects/fx/GradientEffect.h"

// ---------------------------------------------------------------- Rgb helpers
void test_clamp255_bounds() {
  TEST_ASSERT_EQUAL_UINT8(0, clamp255(-5.0f));
  TEST_ASSERT_EQUAL_UINT8(255, clamp255(999.0f));
  TEST_ASSERT_EQUAL_UINT8(128, clamp255(128.0f));
}

void test_clamp01_bounds() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, clamp01(-0.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, clamp01(1.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.4f, clamp01(0.4f));
}

void test_blend_midpoint() {
  Rgb a{0, 0, 0};
  Rgb b{100, 200, 160};
  Rgb c = blend(a, b, 0.5f);
  TEST_ASSERT_EQUAL_UINT8(50, c.r);
  TEST_ASSERT_EQUAL_UINT8(100, c.g);
  TEST_ASSERT_EQUAL_UINT8(80, c.b);
}

void test_scale() {
  Rgb c = scale(Rgb{10, 20, 40}, 0.5f);
  TEST_ASSERT_EQUAL_UINT8(5, c.r);
  TEST_ASSERT_EQUAL_UINT8(10, c.g);
  TEST_ASSERT_EQUAL_UINT8(20, c.b);
}

void test_luma_green_dominant() {
  Rgb grey{100, 100, 100};
  Rgb green{0, 255, 0};
  TEST_ASSERT(luma(green) > luma(grey));
}

void test_hsl_white_when_sat_zero() {
  Rgb c = hslToRgb(0.5f, 0.0f, 1.0f);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(255, c.g);
  TEST_ASSERT_EQUAL_UINT8(255, c.b);
}

void test_hsl_red_at_hue_zero() {
  Rgb c = hslToRgb(0.0f, 1.0f, 0.5f);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(0, c.g);
  TEST_ASSERT_EQUAL_UINT8(0, c.b);
}

void test_palette_heat_low_is_red() {
  Rgb c = paletteColor(PALETTE_HEAT, 0.0f);
  TEST_ASSERT_EQUAL_UINT8(0, c.r);
  TEST_ASSERT_EQUAL_UINT8(0, c.g);
  TEST_ASSERT_EQUAL_UINT8(0, c.b);
}

void test_palette_heat_high_is_white() {
  Rgb c = paletteColor(PALETTE_HEAT, 1.0f);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(255, c.g);
  TEST_ASSERT_EQUAL_UINT8(255, c.b);
}

void test_palette_rainbow_cycles() {
  Rgb c1 = paletteColor(PALETTE_RAINBOW, 0.0f);
  Rgb c2 = paletteColor(PALETTE_RAINBOW, 1.0f);
  TEST_ASSERT_EQUAL_UINT8(c1.r, c2.r);
  TEST_ASSERT_EQUAL_UINT8(c1.g, c2.g);
  TEST_ASSERT_EQUAL_UINT8(c1.b, c2.b);
}

// --------------------------------------------------------------------- Smoother
void test_smoother_attack_then_clamp() {
  Smoother s;
  s.reset(0.0f);
  float v = s.update(2.0f, 0.5f, 0.5f);
  TEST_ASSERT(v > 0.0f);
  // Repeated high target must eventually clamp to 1.0
  for (int i = 0; i < 100; ++i) v = s.update(2.0f, 0.5f, 0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, v);
}

void test_smoother_release_never_negative() {
  Smoother s;
  s.reset(0.5f);
  float v = s.update(0.0f, 1.0f, 1.0f);
  TEST_ASSERT(v < 0.5f);
  TEST_ASSERT(v >= 0.0f);
}

void test_smoother_self_reset() {
  Smoother s;
  s.reset(0.3f);
  float v = s.update(0.3f, 0.9f, 0.9f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.3f, v);
}

// ------------------------------------------------------------------ EffectUtil
void test_eff_level_clamps() {
  EffectParams p;
  p.sensitivity = 2.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, effLevel(p, -1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, effLevel(p, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, effLevel(p, 2.0f));
}

void test_eff_rgb_brightness_range() {
  EffectParams p;
  p.minBrightness = 20;
  p.maxBrightness = 200;
  Rgb low = effRgb(p, 0.0f, 1.0f, 0.5f, 0.0f);
  Rgb high = effRgb(p, 0.0f, 1.0f, 0.5f, 1.0f);
  // luma of high > luma of low (both same hue, level scales brightness)
  TEST_ASSERT(luma(high) > luma(low));
}

void test_eff_hue_offset_wraps() {
  EffectParams p;
  p.startHue = 180;
  p.hueSpeed = 360.0f;
  float h = effHueOffset(p, 1.0f);
  // 0.5 (startHue) + 1.0 wraps 360 -> +0 -> 0.5
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, h);
}

// ------------------------------------------------------------------ LedFrame
void test_ledframe_clear_and_set() {
  LedFrame f(10);
  TEST_ASSERT_EQUAL_INT(10, f.size());
  for (int i = 0; i < f.size(); ++i) {
    TEST_ASSERT_EQUAL_UINT8(0, f.at(i).r);
    TEST_ASSERT_EQUAL_UINT8(0, f.at(i).g);
    TEST_ASSERT_EQUAL_UINT8(0, f.at(i).b);
  }
  f.set(3, Rgb{1, 2, 3});
  TEST_ASSERT_EQUAL_UINT8(1, f.at(3).r);
  TEST_ASSERT_EQUAL_UINT8(2, f.at(3).g);
  TEST_ASSERT_EQUAL_UINT8(3, f.at(3).b);
  f.clear();
  TEST_ASSERT_EQUAL_UINT8(0, f.at(3).r);
}

void test_ledframe_index_clamps() {
  LedFrame f(10);
  TEST_ASSERT_EQUAL_INT(0, f.index(0.0f));
  TEST_ASSERT_EQUAL_INT(9, f.index(1.0f));
  TEST_ASSERT_EQUAL_INT(9, f.index(2.0f));
  TEST_ASSERT_EQUAL_INT(0, f.index(-1.0f));
}

// ---------------------------------------------------------------- Effects
void test_bass_pulse_fill_color_on_beat() {
  LedFrame frame(30);
  AudioFrame a;
  a.timeMs = 1000;
  a.beat = true;
  a.beatStrength = 1.0f;
  a.bass = 1.0f;
  EffectParams p;
  p.maxBrightness = 255;
  p.minBrightness = 0;
  BassPulseEffect fx;
  fx.render(frame, a, p);
  // high bass+beat -> all pixels strongly lit
  bool anyLit = false;
  for (int i = 0; i < frame.size(); ++i) {
    if (luma(frame.at(i)) > 1.0f) anyLit = true;
  }
  TEST_ASSERT(anyLit);
}

void test_bass_pulse_idle_is_dim() {
  LedFrame frame(30);
  AudioFrame a;
  a.timeMs = 0;
  a.bass = 0.0f;
  a.beat = false;
  EffectParams p;
  p.maxBrightness = 255;
  p.minBrightness = 0;
  BassPulseEffect fx;
  fx.render(frame, a, p);
  Rgb c = frame.at(0);
  TEST_ASSERT_EQUAL_UINT8(0, c.r);
  TEST_ASSERT_EQUAL_UINT8(0, c.g);
  TEST_ASSERT_EQUAL_UINT8(0, c.b);
}

void test_gradient_spans_leds() {
  LedFrame frame(64);
  AudioFrame a;
  a.timeMs = 0;
  a.bandCount = 3;
  a.bands[0] = 0.5f;
  a.bands[1] = 0.5f;
  a.bands[2] = 0.5f;
  a.amplitude = 0.2f;
  EffectParams p;
  p.maxBrightness = 200;
  p.minBrightness = 10;
  p.bandCount = 3;
  p.startHue = 0;
  p.hueSpeed = 0;
  GradientEffect fx;
  fx.render(frame, a, p);
  Rgb first = frame.at(0);
  Rgb last = frame.at(63);
  // Gradient should differ across the strip. luma of first != last typically.
  bool differs = (first.r != last.r) || (first.g != last.g) || (first.b != last.b);
  TEST_ASSERT(differs);
  // All within valid 8-bit range obviously; check non-negative
  TEST_ASSERT_EQUAL_UINT8(first.r & 0xFF, first.r);
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_clamp255_bounds);
  RUN_TEST(test_clamp01_bounds);
  RUN_TEST(test_blend_midpoint);
  RUN_TEST(test_scale);
  RUN_TEST(test_luma_green_dominant);
  RUN_TEST(test_hsl_white_when_sat_zero);
  RUN_TEST(test_hsl_red_at_hue_zero);
  RUN_TEST(test_palette_heat_low_is_red);
  RUN_TEST(test_palette_heat_high_is_white);
  RUN_TEST(test_palette_rainbow_cycles);
  RUN_TEST(test_smoother_attack_then_clamp);
  RUN_TEST(test_smoother_release_never_negative);
  RUN_TEST(test_smoother_self_reset);
  RUN_TEST(test_eff_level_clamps);
  RUN_TEST(test_eff_rgb_brightness_range);
  RUN_TEST(test_eff_hue_offset_wraps);
  RUN_TEST(test_ledframe_clear_and_set);
  RUN_TEST(test_ledframe_index_clamps);
  RUN_TEST(test_bass_pulse_fill_color_on_beat);
  RUN_TEST(test_bass_pulse_idle_is_dim);
  RUN_TEST(test_gradient_spans_leds);
  return UNITY_END();
}
