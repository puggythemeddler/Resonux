#include <unity.h>

#include "audio/AudioFrame.h"
#include "cinema/CinematicApply.h"
#include "cinema/CompanionPicker.h"
#include "cinema/CinematicConfig.h"
#include "cinema/CinematicDirector.h"
#include "cinema/CinematicEngine.h"
#include "cinema/SceneAnalyzer.h"
#include "cinema/SceneFrame.h"
#include "cinema/SceneMemory.h"
#include "cinema/SpatialBlock.h"
#include "cinema/SpatialWaveField.h"
#include "cinema/TestInjector.h"

using namespace cine;
using namespace sceneframe;

namespace {

// ---- helpers ---------------------------------------------------------------

AudioFrame frameAt(uint32_t ms, float amp, float bass = 0.0f, float mid = 0.0f,
                   float treble = 0.0f, bool beat = false,
                   float beatStrength = 0.0f) {
  AudioFrame f;
  f.timeMs = ms;
  f.amplitude = amp;
  f.bass = bass;
  f.mid = mid;
  f.treble = treble;
  f.beat = beat;
  f.beatStrength = beatStrength;
  f.bandCount = 9;
  for (int i = 0; i < 9; ++i) f.peaks[i] = amp;
  return f;
}

AudioFeatures quietAudio() {
  AudioFeatures a;
  a.level = 0.02f;
  return a;
}

AudioFeatures loudMusic() {
  AudioFeatures a;
  a.level = 0.9f;
  a.bass = 0.7f;
  a.sustainedLoud = true;
  a.beat = true;
  a.beatStrength = 1.0f;
  return a;
}

Frame videoFrame(uint32_t seq, uint32_t hostMs, SceneKind scene, SceneEvent ev,
                 uint8_t conf, uint8_t lum = 128, uint8_t hue = 128,
                 uint8_t sat = 160, uint8_t val = 160, uint8_t motion = 128) {
  Frame f;
  packFrame(f, seq, hostMs, scene, ev, conf, lum, hue, sat, val, motion, 128, 0);
  return f;
}

// --------------------------------------------------------------- SceneFrame
void test_codec_roundtrip() {
  Frame f;
  packFrame(f, 42u, 123456u, SCENE_ACTION, SEVENT_BOOM, 77, 90, 30, 200, 90,
            200, 150, SFLAG_PROGRAM_AUDIO);
  TEST_ASSERT_TRUE(validFrame(f, sizeof(f)));

  Frame g{};
  memcpy(&g, &f, sizeof(f));
  TEST_ASSERT_EQUAL_UINT32(42u, g.seq);
  TEST_ASSERT_EQUAL_UINT32(123456u, g.hostTimeMs);
  TEST_ASSERT_EQUAL_UINT8(SCENE_ACTION, g.sceneId);
  TEST_ASSERT_EQUAL_UINT8(SEVENT_BOOM, g.eventId);
  TEST_ASSERT_EQUAL_UINT8(90u, g.avgLuminance);
  TEST_ASSERT_EQUAL_UINT8(30u, g.hue);
  TEST_ASSERT_EQUAL_UINT8(1u, g.sourceFlags & SFLAG_PROGRAM_AUDIO);
}

void test_codec_rejects_bad_input() {
  Frame f;
  packFrame(f, 1u, 2u, SCENE_SPEECH, SEVENT_NONE, 50, 128, 90, 100, 100, 0, 0, 0);
  TEST_ASSERT_TRUE(validFrame(f, sizeof(f)));

  // truncated payload
  TEST_ASSERT_FALSE(validFrame(f, sizeof(f) - 1));
  // bad magic
  Frame bad = f;
  bad.magic = 0xDEADBEEFu;
  TEST_ASSERT_FALSE(validFrame(bad, sizeof(bad)));
  // wrong version
  bad = f;
  bad.version = 2;
  TEST_ASSERT_FALSE(validFrame(bad, sizeof(bad)));
  // out-of-range scene / event ids
  bad = f;
  bad.sceneId = (uint8_t)(SCENE_MUSIC + 1);
  TEST_ASSERT_FALSE(validFrame(bad, sizeof(bad)));
  bad = f;
  bad.eventId = (uint8_t)(SEVENT_CHANGE + 1);
  TEST_ASSERT_FALSE(validFrame(bad, sizeof(bad)));
  // confidence is clamped, never trusted raw
  Frame big;
  packFrame(big, 1u, 2u, SCENE_SPEECH, SEVENT_NONE, 250, 128, 90, 100, 100, 0, 0, 0);
  TEST_ASSERT_EQUAL_UINT8(100u, big.eventConfidence);
}

void test_codec_idents() {
  TEST_ASSERT_EQUAL_STRING("boom", eventIdent(SEVENT_BOOM));
  TEST_ASSERT_EQUAL_STRING("chase", sceneIdent(SCENE_CHASE));
  TEST_ASSERT_EQUAL_STRING("music", sceneIdent(SCENE_MUSIC));
  TEST_ASSERT_EQUAL_STRING("none", eventIdent(SEVENT_NONE));
}

// --------------------------------------------------------------- CinematicConfig
void test_presets_tune_knobs_and_keep_network() {
  Config c;
  defaultConfig(c);
  c.receiveUdp = true;
  c.port = 9000;
  c.group[0] = 'x';
  c.group[1] = '\0';

  applyPreset(c, MODE_SUBTLE);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.45f, c.reaction);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, c.speed);
  TEST_ASSERT_EQUAL_INT(MODE_SUBTLE, c.mode);

  applyPreset(c, MODE_EXTREME);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.25f, c.reaction);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, c.flashIntensity);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, c.smoothing);

  // network + enabled survived both presets
  TEST_ASSERT_TRUE(c.receiveUdp);
  TEST_ASSERT_EQUAL_INT(9000, (int)c.port);
  TEST_ASSERT_EQUAL_CHAR('x', c.group[0]);
}

void test_clamp_normalizes_garbage() {
  Config c;
  c.enabled = true;
  c.mode = 99;
  c.genre = -3;
  c.sensitivity = 999.0f;
  c.reaction = -1.0f;
  c.whisperDim = 2.0f;
  c.port = 0;
  c.staleMs = 65535;
  c.smoothing = NAN;
  clampConfig(c);
  TEST_ASSERT_EQUAL_INT(MODE_EXTREME, c.mode);   // clamped to count-1
  TEST_ASSERT_EQUAL_INT(GENRE_NONE, c.genre);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, c.sensitivity);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c.reaction);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.whisperDim);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c.smoothing);  // NaN -> lo bound
  TEST_ASSERT(c.port >= 1024);
  TEST_ASSERT(c.staleMs <= 10000);
}

// --------------------------------------------------------------- SceneAnalyzer
void test_analyzer_boom_detection_and_cooldown() {
  SceneAnalyzer an;
  // phase 1: quiet baseline so a later bass jump stands out
  for (uint32_t ms = 0; ms < 60 * 16u; ms += 16)
    an.process(frameAt(ms, 0.05f, 0.05f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, an.latest().boom);

  // phase 2: one loud bass frame -> boom triggers
  uint32_t ms = 60 * 16u;
  AudioFeatures a = an.process(frameAt(ms, 0.8f, 0.8f));
  TEST_ASSERT_TRUE(a.boom > 0.3f);
  const float firstBoom = a.boom;

  // phase 3: immediate re-trigger is blocked by the cooldown
  for (int i = 0; i < 20; ++i) {
    ms += 16;
    a = an.process(frameAt(ms, 0.8f, 0.8f));
    if (a.boom > firstBoom + 0.05f) TEST_FAIL_MESSAGE("boom re-triggered early");
  }
}

void test_analyzer_sustained_loud_gates_booms() {
  SceneAnalyzer an;
  // quiet baseline
  for (uint32_t ms = 0; ms < 60 * 16u; ms += 16)
    an.process(frameAt(ms, 0.05f, 0.05f));
  // sustained loud music: level rises, stays > 0.55 for >= 1200 ms
  AudioFeatures a;
  for (uint32_t ms = 2000; ms < 2000 + 160 * 16u; ms += 16) {
    a = an.process(frameAt(ms, 0.9f, 0.7f));
  }
  TEST_ASSERT_TRUE(a.sustainedLoud);
  TEST_ASSERT_FLOAT_WITHIN(0.3f, 0.0f, a.boom);  // no boom spam on music
}

void test_analyzer_whisper_and_silence() {
  SceneAnalyzer an;
  AudioFeatures a;
  for (uint32_t ms = 0; ms < 60 * 16u; ms += 16)
    a = an.process(frameAt(ms, 0.08f, 0.02f));  // quiet but audible
  TEST_ASSERT_TRUE(a.whisper);
  TEST_ASSERT_FALSE(a.silence);

  SceneAnalyzer silent;
  uint32_t ms = 0;
  for (int i = 0; i < 30; ++i, ms += 16)
    silent.process(frameAt(ms, 0.01f, 0.0f));
  TEST_ASSERT_TRUE(silent.latest().silence);
  TEST_ASSERT_FALSE(silent.latest().whisper);
}

void test_analyzer_dialogue() {
  SceneAnalyzer an;
  AudioFeatures a;
  for (uint32_t ms = 0; ms < 40 * 16u; ms += 16)
    a = an.process(frameAt(ms, 0.3f, 0.05f, 0.5f));  // mid-heavy, moderate level
  TEST_ASSERT_TRUE(a.dialogue);
}

void test_analyzer_tension_builds_and_collapses() {
  SceneAnalyzer an;
  // creep: sustained mid + treble at moderate-low level
  float tensionAtQuiet = 0.0f;
  for (uint32_t ms = 0; ms < 120 * 16u; ms += 16) {
    AudioFeatures a = an.process(frameAt(ms, 0.3f, 0.1f, 0.35f, 0.2f));
    if (ms > 100 * 16u) tensionAtQuiet = a.tension;
  }
  TEST_ASSERT_TRUE(tensionAtQuiet > 0.3f);

  // loud moment breaks the tension
  AudioFeatures a = an.process(frameAt(150 * 16u, 0.9f, 0.8f, 0.8f, 0.8f));
  TEST_ASSERT_TRUE(a.tension < tensionAtQuiet * 0.7f);
}

// --------------------------------------------------------------- CinematicEngine
void test_engine_audio_only_fallback() {
  Config c;
  defaultConfig(c);
  CinematicEngine eng;
  eng.configure(c);

  for (uint32_t now = 0; now < 5000; now += 16)
    eng.update(quietAudio(), nullptr, now);

  TEST_ASSERT_EQUAL_INT(SRC_LOCAL_AUDIO, (int)eng.status().source);
  TEST_ASSERT_FALSE(eng.status().companionAlive);
  TEST_ASSERT_EQUAL_INT(SCENE_UNDEFINED, (int)eng.status().scene);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, eng.look().flash);
}

void test_engine_video_primary_fusion() {
  Config c;
  defaultConfig(c);
  c.visualInfluence = 0.9f;
  CinematicEngine eng;
  eng.configure(c);

  Frame v = videoFrame(1u, 1000u, SCENE_ACTION, SEVENT_NONE, 80, 200, 40, 180, 180, 220);
  uint32_t now = 1000;
  eng.update(quietAudio(), &v, now);
  for (int i = 0; i < 16; ++i) {
    now += 16;
    eng.update(quietAudio(), &v, now);  // audio quiet -> video-only
  }

  TEST_ASSERT_TRUE(eng.status().companionAlive);
  TEST_ASSERT_EQUAL_INT(SCENE_ACTION, (int)eng.status().scene);
  TEST_ASSERT_EQUAL_INT(SRC_VIDEO, (int)eng.status().source);
  TEST_ASSERT_TRUE(eng.status().motion > 175);
  TEST_ASSERT_TRUE(eng.status().luminance > 190);

  // audio now present -> fused
  AudioFeatures a = quietAudio();
  a.level = 0.5f;
  now += 16;
  eng.update(a, &v, now);
  TEST_ASSERT_EQUAL_INT(SRC_FUSED, (int)eng.status().source);
}

void test_engine_flash_envelope_and_min_gap() {
  Config c;
  defaultConfig(c);
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame none = videoFrame(1u, now, SCENE_ACTION, SEVENT_NONE, 100, 128, 40, 160, 160, 128);
  Frame flash = videoFrame(2u, now, SCENE_ACTION, SEVENT_FLASH, 100, 255, 40, 160, 160, 200);

  eng.update(quietAudio(), &none, now);
  now += 16;
  eng.update(quietAudio(), &flash, now);
  const float peak = eng.look().flash;
  TEST_ASSERT_TRUE(peak > 0.4f);
  TEST_ASSERT_TRUE(eng.look().flash <= c.flashIntensity + 0.001f);

  // continuous flash frames inside the min-gap do not stack higher
  for (int i = 0; i < 20; ++i) {
    now += 16;
    eng.update(quietAudio(), &flash, now);
    TEST_ASSERT_TRUE(eng.look().flash <= peak + 0.05f);
  }

  // once the feed reports "none", the envelope decays toward zero
  for (int i = 0; i < 400; ++i) {
    now += 16;
    eng.update(quietAudio(), &none, now);
  }
  TEST_ASSERT_TRUE(eng.look().flash < 0.01f);
}

void test_engine_boom_event_cooldown() {
  Config c;
  defaultConfig(c);
  c.boomCooldownMs = 2000;  // long window so the cooldown is observable
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame none = videoFrame(1u, now, SCENE_EXPLOSION, SEVENT_NONE, 100, 128, 30, 200, 200, 128);
  Frame boom = videoFrame(2u, now, SCENE_EXPLOSION, SEVENT_BOOM, 100, 255, 30, 200, 200, 255);

  eng.update(quietAudio(), &none, now);
  now += 16;
  eng.update(quietAudio(), &boom, now);
  const float firstImpact = eng.look().impact;
  TEST_ASSERT_TRUE(firstImpact > 0.4f);

  // let the envelope fully decay (t ~1.6 s), still INSIDE the 2 s cooldown
  for (int i = 0; i < 100; ++i) {
    now += 16;
    eng.update(quietAudio(), &none, now);
  }
  TEST_ASSERT_TRUE(eng.look().impact < 0.05f);

  // an early repeat must NOT retrigger while the cooldown is active
  now += 16;
  eng.update(quietAudio(), &boom, now);
  TEST_ASSERT_TRUE(eng.look().impact < 0.05f);

  // teleport past the cooldown, then a fresh boom retriggers
  now += 500;
  eng.update(quietAudio(), &none, now);
  now += 16;
  eng.update(quietAudio(), &boom, now);
  TEST_ASSERT_TRUE(eng.look().impact > 0.5f);
}

void test_engine_sustained_music_pulses_but_loud_quiet() {
  Config c;
  defaultConfig(c);
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  for (int i = 0; i < 60; ++i) {
    now += 16;
    eng.update(loudMusic(), nullptr, now);  // no companion at all
  }
  // music: no flash spam, no boom envelope, just pulse + motion
  TEST_ASSERT_EQUAL_INT(SRC_LOCAL_AUDIO, (int)eng.status().source);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, eng.look().flash);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, eng.look().impact);
  TEST_ASSERT_TRUE(eng.look().pulse > 0.0f);
}

void test_engine_failsafe_when_feed_goes_stale() {
  Config c;
  defaultConfig(c);
  c.staleMs = 1200;
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame v = videoFrame(1u, now, SCENE_ACTION, SEVENT_NONE, 90, 200, 60, 200, 200, 200);
  for (int i = 0; i < 10; ++i) {
    now += 16;
    eng.update(quietAudio(), &v, now);
  }
  TEST_ASSERT_TRUE(eng.status().companionAlive);

  // companion vanishes -> engine falls back within staleMs
  for (int i = 0; i < 90; ++i) {
    now += 16;
    eng.update(quietAudio(), nullptr, now);
  }
  TEST_ASSERT_FALSE(eng.status().companionAlive);
  TEST_ASSERT_EQUAL_INT(SRC_LOCAL_AUDIO, (int)eng.status().source);

  // long after the feed died, the scene memory is fully dropped
  for (int i = 0; i < 300; ++i) {
    now += 16;
    eng.update(quietAudio(), nullptr, now);
  }
  TEST_ASSERT_EQUAL_INT(SCENE_UNDEFINED, (int)eng.status().scene);
}

void test_engine_whisper_dims_and_dark_floor() {
  Config c;
  defaultConfig(c);
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame wis = videoFrame(1u, now, SCENE_SPEECH, SEVENT_WHISPER, 100, 60, 190, 90, 60, 20);
  for (int i = 0; i < 8; ++i) {
    now += 16;
    eng.update(quietAudio(), &wis, now);
  }
  TEST_ASSERT_TRUE(eng.look().calm > 0.5f);
  TEST_ASSERT_TRUE(eng.look().brightness < c.maxBrightness - 0.1f);
  TEST_ASSERT_TRUE(eng.look().hueShift > 0.0f);  // cooled

  // dark scene: brightness dips but never below the ambient floor
  Frame dark = videoFrame(2u, now, SCENE_QUIET, SEVENT_DARK, 100, 5, 200, 60, 20, 5);
  for (int i = 0; i < 8; ++i) {
    now += 16;
    eng.update(quietAudio(), &dark, now);
  }
  TEST_ASSERT_TRUE(eng.look().brightness < c.maxBrightness);
  TEST_ASSERT_TRUE(eng.look().brightness >= c.ambientFloor - 0.001f);
}

void test_engine_video_colour_tint() {
  Config c;
  defaultConfig(c);
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame v = videoFrame(1u, now, SCENE_ACTION, SEVENT_NONE, 90, 128, 200 /*hue*/, 50, 200, 128);
  for (int i = 0; i < 12; ++i) {
    now += 16;
    eng.update(quietAudio(), &v, now);
  }
  TEST_ASSERT_TRUE(eng.look().tintMix > 0.2f);
  // orange-leaning tint (hue ~200/255*360 ~ 282° -> purple/pink, sat/val high)
  TEST_ASSERT_TRUE(eng.look().tint.r > 0 || eng.look().tint.b > 0);
  // with no companion there is no tint
  now = 6000;
  eng.update(quietAudio(), nullptr, now);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, eng.look().tintMix);
}

void test_engine_genre_horror_dims() {
  uint32_t now = 0;
  AudioFeatures a;
  a.level = 0.5f;
  a.bass = 0.4f;
  a.mid = 0.4f;
  a.sustainedLoud = false;

  Config neutral;
  defaultConfig(neutral);
  neutral.genre = GENRE_NONE;
  CinematicEngine en;
  en.configure(neutral);
  for (int i = 0; i < 30; ++i) {
    now += 16;
    en.update(a, nullptr, now);
  }
  const float brightNeutral = en.look().brightness;

  now = 2000;
  Config horror;
  defaultConfig(horror);
  horror.genre = GENRE_HORROR;
  CinematicEngine eh;
  eh.configure(horror);
  for (int i = 0; i < 30; ++i) {
    now += 16;
    eh.update(a, nullptr, now);
  }
  TEST_ASSERT_TRUE(eh.look().brightness < brightNeutral * 0.95f);
}

void test_engine_source_enum_ordering() {
  // keep the enum stable for the web API ids
  TEST_ASSERT_EQUAL_INT(0, SRC_LOCAL_AUDIO);
  TEST_ASSERT_EQUAL_INT(1, SRC_VIDEO);
  TEST_ASSERT_EQUAL_INT(2, SRC_FUSED);
}

// ------------------------------------------------------------- applyToThemeFrame
void test_apply_to_theme_frame() {
  Themes::ThemeFrame th;
  th.brightness = 1.0f;
  th.saturation = 1.0f;
  th.intensity = 1.0f;
  th.paletteCount = 1;
  Rgb pal[1] = {{10, 20, 30}};
  th.palette = pal;

  Config c;
  defaultConfig(c);
  Look lk;
  lk.brightness = 0.5f;
  lk.hueShift = 0.1f;
  lk.flash = 0.8f;
  lk.calm = 0.4f;
  lk.saturation = 0.7f;
  lk.tintMix = 0.8f;
  lk.tint = Rgb{255, 0, 0};

  applyToThemeFrame(th, lk, c);

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, th.brightness);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, th.saturation);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, th.colourShift);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.44f, th.beatFlash);  // flash * 0.55
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.8f, th.intensity);  // 1 * (1-0.4*0.5)
  TEST_ASSERT_TRUE(th.primary.r > 200);  // blended hard toward red
  TEST_ASSERT_TRUE(th.primary.b < 40);
}

// ---------------------------------------------------------------- scene memory
void test_memory_labels_and_genre_auto() {
  TEST_ASSERT_EQUAL_STRING("subtle", modeIdent(MODE_SUBTLE));
  TEST_ASSERT_EQUAL_STRING("Gentle", modeLabel(MODE_SUBTLE));
  TEST_ASSERT_EQUAL_STRING("auto", genreIdent(GENRE_AUTO));
  TEST_ASSERT_EQUAL_STRING("Automatic", genreLabel(GENRE_AUTO));
  TEST_ASSERT_EQUAL_STRING("suspense", moodIdent(MOOD_SUSPENSE));
  TEST_ASSERT_EQUAL_STRING("Tension", moodLabel(MOOD_TENSION));
  TEST_ASSERT_EQUAL_INT(MOOD_PERFORMANCE, MOOD_COUNT - 1);
}

void test_memory_hysteresis_switches_after_repeated_votes() {
  SceneMemory m;
  uint32_t now = 0;
  auto feed = [&](sceneframe::SceneKind k, uint32_t& n) { m.feed(k, sceneframe::SEVENT_NONE, 80, 120, n); n += 16; };

  for (int i = 0; i < 5; ++i) feed(sceneframe::SCENE_SPEECH, now);
  TEST_ASSERT_EQUAL_INT(sceneframe::SCENE_SPEECH, (int)m.context().current);

  // two sporadic ACTION votes do not flip the classification
  feed(sceneframe::SCENE_ACTION, now);
  feed(sceneframe::SCENE_ACTION, now);
  TEST_ASSERT_EQUAL_INT(sceneframe::SCENE_SPEECH, (int)m.context().current);

  // the third consecutive vote flips it
  feed(sceneframe::SCENE_ACTION, now);
  TEST_ASSERT_EQUAL_INT(sceneframe::SCENE_ACTION, (int)m.context().current);
  TEST_ASSERT_EQUAL_INT(sceneframe::SCENE_SPEECH, (int)m.context().previous);
}

void test_memory_change_event_bypasses_hysteresis() {
  SceneMemory m;
  uint32_t now = 0;
  for (int i = 0; i < 5; ++i) { m.feed(sceneframe::SCENE_SPEECH, sceneframe::SEVENT_NONE, 80, 120, now); now += 16; }
  m.feed(sceneframe::SCENE_CHASE, sceneframe::SEVENT_CHANGE, 90, 200, now);
  TEST_ASSERT_EQUAL_INT(sceneframe::SCENE_CHASE, (int)m.context().current);
  TEST_ASSERT_EQUAL_INT(sceneframe::SCENE_SPEECH, (int)m.context().previous);
}

void test_memory_dwell_and_transition_timers() {
  SceneMemory m;
  uint32_t now = 0;
  for (int i = 0; i < 5; ++i) { m.feed(sceneframe::SCENE_SPEECH, sceneframe::SEVENT_NONE, 80, 120, now); now += 16; }
  for (int i = 0; i < 10; ++i) { m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_NONE, 80, 120, now); now += 16; }
  SceneContext c = m.context();
  TEST_ASSERT_EQUAL_INT(sceneframe::SCENE_ACTION, (int)c.current);
  TEST_ASSERT(c.dwellMs > 100);
  TEST_ASSERT(c.sinceTransitionMs > 100);
}

void test_memory_energy_rises_with_events_and_decays() {
  SceneMemory m;
  uint32_t now = 0;
  for (int i = 0; i < 20; ++i) { m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_NONE, 0, 5, now); now += 16; }
  SceneContext a = m.context();
  TEST_ASSERT(a.moodEnergy < 0.2f);
  TEST_ASSERT_FALSE(a.risingEnergy);
  TEST_ASSERT_EQUAL_UINT8(0, a.recentEventCount);

  // a boom event with bright luminance pushes energy up
  m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_BOOM, 90, 200, now);
  now += 16;
  m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_BOOM, 90, 200, now);
  SceneContext b = m.context();
  TEST_ASSERT(b.moodEnergy > a.moodEnergy);
  TEST_ASSERT_TRUE(b.risingEnergy);
  TEST_ASSERT(b.recentEventCount >= 1);

  // long quiet decays energy and ages the events out of the window
  for (int i = 0; i < 500; ++i) { m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_NONE, 0, 5, now); now += 16; }
  SceneContext c = m.context();
  TEST_ASSERT(c.moodEnergy < b.moodEnergy);
  TEST_ASSERT_EQUAL_UINT8(0, c.recentEventCount);
}

void test_memory_ring_bounded_and_coalesced() {
  SceneMemory m;
  uint32_t now = 0;
  // 30 distinct events, spaced > 250 ms apart -> ring saturates at depth
  for (int i = 0; i < 30; ++i) { m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_BOOM, 90, 200, now); now += 300; }
  TEST_ASSERT(m.count() <= kSceneMemoryDepth);

  SceneMemory c;
  c.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_BOOM, 90, 200, now);
  now += 100;
  c.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_BOOM, 90, 200, now);
  TEST_ASSERT_EQUAL_INT(1, c.count());  // coalesced, no ring spam
}

// ---------------------------------------------------------------- director
void test_director_mood_cascade() {
  Config cfg;
  defaultConfig(cfg);
  AudioFeatures a;
  a.level = 0.2f;
  a.tension = 0.5f;
  SceneMemory m;
  CinematicDirector dir;
  uint32_t now = 0;

  // phase 1: quiet dark -> calm
  for (int i = 0; i < 10; ++i) { m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_NONE, 0, 5, now); now += 16; }
  TEST_ASSERT_EQUAL_INT(MOOD_CALM, (int)dir.compute(m.context(), a, nullptr, cfg, now).mood);

  // phase 2: whisper build -> suspense/tension
  for (int i = 0; i < 30; ++i) { m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_WHISPER, 70, 60, now); now += 16; }
  Mood built = dir.compute(m.context(), a, nullptr, cfg, now).mood;
  TEST_ASSERT_TRUE(built == MOOD_SUSPENSE || built == MOOD_TENSION);

  // phase 3: flash -> impact
  Frame fl = videoFrame(2u, now, sceneframe::SCENE_ACTION, sceneframe::SEVENT_FLASH, 100, 200, 40, 180, 180, 200);
  m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_FLASH, 100, 200, now);
  TEST_ASSERT_EQUAL_INT(MOOD_IMPACT, (int)dir.compute(m.context(), a, &fl, cfg, now).mood);

  // phase 4: aftermath shortly after the impact (hold lasts 400 ms, so keep
  // feeding quietly past it before expecting the mood to relax)
  Frame calm2 = videoFrame(3u, now, sceneframe::SCENE_ACTION, sceneframe::SEVENT_NONE, 0, 200, 40, 180, 180, 40);
  for (int i = 0; i < 30; ++i) { now += 16; m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_NONE, 0, 120, now); }
  Mood post = dir.compute(m.context(), a, &calm2, cfg, now).mood;
  TEST_ASSERT_TRUE(post == MOOD_AFTERMATH || post == MOOD_CALM);

  // phase 5: long quiet returns to calm
  for (int i = 0; i < 200; ++i) { now += 16; m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_NONE, 0, 5, now); }
  TEST_ASSERT_EQUAL_INT(MOOD_CALM, (int)dir.compute(m.context(), a, nullptr, cfg, now).mood);
}

void test_director_hold_prevents_flicker() {
  Config cfg;
  defaultConfig(cfg);
  AudioFeatures a;
  a.level = 0.15f;
  SceneMemory m;
  CinematicDirector dir;
  uint32_t now = 1000;

  for (int i = 0; i < 10; ++i) { m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_NONE, 0, 5, now); now += 16; }
  TEST_ASSERT_EQUAL_INT(MOOD_CALM, (int)dir.compute(m.context(), a, nullptr, cfg, now).mood);

  // a lift toward SUSPENSE arrives inside the 250 ms hold: a luminance ramp
  // keeps energy climbing every tick, but the hold keeps us CALM until it elapses
  now = 1300;
  int lum = 60;
  for (int i = 0; i < 4; ++i) {
    m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_WHISPER, 70, (uint8_t)lum, now);
    lum += 30;
    now += 16;
  }
  Mood gated = dir.compute(m.context(), a, nullptr, cfg, now).mood;
  TEST_ASSERT_EQUAL_INT(MOOD_CALM, (int)gated);

  // after the hold expires the lift lands
  now = 1500;
  m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_WHISPER, 70, 210, now);
  Mood switched = dir.compute(m.context(), a, nullptr, cfg, now).mood;
  TEST_ASSERT_EQUAL_INT(MOOD_SUSPENSE, (int)switched);

  // a discrete IMPACT punches straight through the hold — never late, never dropped
  Frame fl = videoFrame(2u, now, sceneframe::SCENE_ACTION, sceneframe::SEVENT_FLASH, 100, 200, 40, 180, 180, 200);
  m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_FLASH, 100, 200, now + 30);
  Mood punched = dir.compute(m.context(), a, &fl, cfg, now + 30).mood;
  TEST_ASSERT_EQUAL_INT(MOOD_IMPACT, (int)punched);
}

void test_director_genre_auto_stays_bounded() {
  Config cfg;
  defaultConfig(cfg);
  cfg.genre = GENRE_AUTO;
  AudioFeatures a;
  a.level = 0.2f;
  a.tension = 0.8f;
  SceneMemory m;
  CinematicDirector dir;
  uint32_t now = 0;
  for (int i = 0; i < 30; ++i) { m.feed(sceneframe::SCENE_QUIET, sceneframe::SEVENT_NONE, 0, 40, now); now += 16; }
  CinematicIntent in = dir.compute(m.context(), a, nullptr, cfg, now);
  TEST_ASSERT(in.mood >= MOOD_CALM && in.mood < MOOD_COUNT);
  TEST_ASSERT(in.flashScale > 0.0f && in.flashScale <= 1.0f);
  TEST_ASSERT(in.pulseDepth > 0.0f && in.pulseDepth <= 1.5f);
}

void test_director_flash_scale_never_exceeds_one() {
  Config cfg;
  defaultConfig(cfg);
  AudioFeatures a;
  a.level = 0.3f;
  SceneMemory m;
  CinematicDirector dir;
  uint32_t now = 0;
  // drive toward high-energy action and confirm the ceiling holds
  for (int i = 0; i < 40; ++i) { m.feed(sceneframe::SCENE_ACTION, sceneframe::SEVENT_FLASH, 100, 240, now); now += 16; }
  Frame fl = videoFrame(1u, now, sceneframe::SCENE_ACTION, sceneframe::SEVENT_FLASH, 100, 240, 40, 180, 180, 240);
  CinematicIntent in = dir.compute(m.context(), a, &fl, cfg, now);
  TEST_ASSERT(in.flashScale <= 1.0f);
}

void test_engine_status_exposes_intent() {
  Config c;
  defaultConfig(c);
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame v = videoFrame(1u, now, sceneframe::SCENE_EXPLOSION, sceneframe::SEVENT_BOOM, 100, 255, 30, 200, 200, 255);
  eng.update(quietAudio(), &v, now);
  now += 16;
  eng.update(quietAudio(), &v, now);

  TEST_ASSERT_EQUAL_INT(MOOD_IMPACT, (int)eng.status().mood);
  TEST_ASSERT(eng.status().moodEnergy >= 0.0f && eng.status().moodEnergy <= 1.0f);
  TEST_ASSERT(eng.status().recentEvents >= 1);
}

// ------------------------------------------------------- spatial block codec
void test_spatial_block_roundtrip() {
  SpatialInfo si;
  si.focusX = 60;
  si.focusY = 200;
  si.zoneCount = 3;
  si.zones[0] = SpatialInfo::Zone{ZONE_LEFT, 40};
  si.zones[1] = SpatialInfo::Zone{ZONE_CENTER, 35};
  si.zones[2] = SpatialInfo::Zone{ZONE_FULLSCREEN, 90};

  uint8_t buf[sceneframe::kSpatialMaxBlock];
  const size_t total = packSpatial(buf, sizeof(buf), si);

  // 3 header + 2 focus + 6 zone bytes
  TEST_ASSERT_EQUAL_UINT32(11u, (uint32_t)total);
  TEST_ASSERT_EQUAL_UINT8(kSpatialMagic0, buf[0]);
  TEST_ASSERT_EQUAL_UINT8(kSpatialMagic1, buf[1]);
  TEST_ASSERT_EQUAL_UINT8(8u, buf[2]);  // blockLen = 2 + 2*3

  SpatialInfo out;
  TEST_ASSERT_TRUE(parseSpatial(buf, total, out));
  TEST_ASSERT_EQUAL_UINT8(60u, out.focusX);
  TEST_ASSERT_EQUAL_UINT8(200u, out.focusY);
  TEST_ASSERT_EQUAL_UINT8(3u, out.zoneCount);
  TEST_ASSERT_EQUAL_UINT8(ZONE_LEFT, out.zones[0].zone);
  TEST_ASSERT_EQUAL_UINT8(40u, out.zones[0].confidence);
  TEST_ASSERT_EQUAL_UINT8(ZONE_FULLSCREEN, out.zones[2].zone);
  TEST_ASSERT_EQUAL_UINT8(90u, out.zones[2].confidence);
}

void test_spatial_block_rides_an_overlong_frame() {
  Frame f;
  packFrame(f, 42u, 7u, SCENE_ACTION, SEVENT_BOOM, 80, 200, 40, 180, 180, 220,
            128, 0, SFLAG_SPATIAL_BLOCK);

  SpatialInfo si;
  si.focusX = 255;
  si.focusY = 0;
  si.zoneCount = 1;
  si.zones[0] = SpatialInfo::Zone{ZONE_RIGHT, 75};
  uint8_t block[sceneframe::kSpatialMaxBlock];
  const size_t blen = packSpatial(block, sizeof(block), si);

  uint8_t wire[sceneframe::kSpatialMaxBlock + sizeof(sceneframe::Frame)];
  memcpy(wire, &f, sizeof(f));
  memcpy(wire + sizeof(f), block, blen);
  const size_t total = sizeof(f) + blen;

  // the base frame stays valid (over-long payloads are accepted) ...
  TEST_ASSERT_TRUE(validFrame(f, total));
  // ... and the block parses right after the base struct
  SpatialInfo out;
  TEST_ASSERT_TRUE(parseSpatial(wire + sizeof(f), blen, out));
  TEST_ASSERT_EQUAL_UINT8(255u, out.focusX);
  TEST_ASSERT_EQUAL_UINT8(ZONE_RIGHT, out.zones[0].zone);
}

void test_spatial_block_rejects_malformed() {
  uint8_t buf[sceneframe::kSpatialMaxBlock] = {0};
  SpatialInfo si;
  si.focusX = 100;
  si.focusY = 100;
  si.zoneCount = 2;
  packSpatial(buf, sizeof(buf), si);
  SpatialInfo out;

  // bad magic
  uint8_t bad[sceneframe::kSpatialMaxBlock];
  memcpy(bad, buf, sizeof(buf));
  bad[0] = 'X';
  TEST_ASSERT_FALSE(parseSpatial(bad, sizeof(bad), out));
  // truncated (fewer bytes than the block needs)
  TEST_ASSERT_FALSE(parseSpatial(buf, 4, out));  // len=6 needs 6 avail
  // odd blockLen
  memcpy(bad, buf, sizeof(buf));
  bad[2] = 5;
  TEST_ASSERT_FALSE(parseSpatial(bad, sizeof(bad), out));
  // oversized blockLen
  bad[2] = 2 + 2 * (kSpatialMaxEntries + 1);
  TEST_ASSERT_FALSE(parseSpatial(bad, sizeof(bad), out));
  // unknown zone id inside the pairs
  memcpy(bad, buf, sizeof(buf));
  bad[2] = 4;  // one entry
  bad[5] = (uint8_t)(ZONE_COUNT + 3);
  TEST_ASSERT_FALSE(parseSpatial(bad, 7, out));
  // empty zero-length block is also malformed (need at least focus bytes)
  memcpy(bad, buf, sizeof(buf));
  bad[2] = 1;
  TEST_ASSERT_FALSE(parseSpatial(bad, 4, out));
}

void test_spatial_zone_idents() {
  TEST_ASSERT_EQUAL_STRING("left", zoneIdent(ZONE_LEFT));
  TEST_ASSERT_EQUAL_STRING("screen", zoneIdent(ZONE_FULLSCREEN));
  TEST_ASSERT_EQUAL_STRING("Centre", zoneLabel(ZONE_CENTER));
  TEST_ASSERT_EQUAL_STRING("Bottom-left", zoneLabel(ZONE_BOTTOM_LEFT));
  TEST_ASSERT_EQUAL_STRING("none", zoneIdent((ZoneId)99));
}

// --------------------------------------------------------- spatial wave field
void test_wave_rest_is_zero_and_peaks_at_spawn() {
  SpatialWaveField f;
  TEST_ASSERT_EQUAL_INT(0, f.activeCount());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, f.intensityAt(0.5f, 0.5f, 0u));

  f.spawn(0.5f, 0.5f, 1.0f, 2.0f, 0.8f, 0.7f, 0u);
  TEST_ASSERT_EQUAL_INT(1, f.activeCount());
  const float nearOrigin = f.intensityAt(0.5f, 0.5f, 1u);
  const float midway = f.intensityAt(0.9f, 0.5f, 1u);
  const float far = f.intensityAt(0.05f, 0.05f, 1u);
  TEST_ASSERT(nearOrigin > 0.9f);
  TEST_ASSERT(nearOrigin > midway && midway > far);
  TEST_ASSERT(f.intensityAt(1.0f, 1.0f, 1u) < 0.6f);
}

void test_wave_propagates_away_then_fades() {
  SpatialWaveField f;
  f.spawn(0.2f, 0.5f, 1.0f, 1.0f, 0.8f, 0.4f, 0u);
  const float s0 = f.intensityAt(0.8f, 0.5f, 0u);       // far ahead, not there yet
  const float sMid = f.intensityAt(0.8f, 0.5f, 500u);   // front arrives
  const float sLate = f.intensityAt(0.8f, 0.5f, 1500u); // passed + decayed
  TEST_ASSERT(sMid > s0);
  TEST_ASSERT(sMid > sLate);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, f.intensityAt(0.2f, 0.5f, 0u));
  TEST_ASSERT(f.intensityAt(0.2f, 0.5f, 1000u) < 0.2f);  // origin left behind
}

void test_wave_ring_is_bounded_and_overwrites() {
  SpatialWaveField f;
  TEST_ASSERT_EQUAL_INT(12, f.maxWaves());
  f.setMaxWaves(4);
  for (int i = 0; i < 10; ++i)
    f.spawn(0.1f + 0.08f * i, 0.5f, 0.5f, 2.0f, 0.8f, 0.7f, (uint32_t)i);
  TEST_ASSERT_EQUAL_INT(4, f.activeCount());  // bounded: oldest are displaced
  f.reset();
  TEST_ASSERT_EQUAL_INT(0, f.activeCount());
  f.spawn(0.5f, 0.5f, 1.0f, 2.0f, 0.8f, 0.7f, 0u);
  TEST_ASSERT_EQUAL_INT(1, f.activeCount());
  TEST_ASSERT(f.intensityAt(0.5f, 0.5f, 1u) > 0.9f);  // fresh ring slot responds
}

void test_wave_line_sweep_lights_along_its_path() {
  SpatialWaveField f;
  f.spawnLine(0.2f, 0.5f, 1.0f, 0.0f, 1.0f, 1.0f, 0.8f, 0.4f, 0u);
  // the front takes time to reach the east side of the room
  const float east0 = f.intensityAt(0.8f, 0.5f, 0u);
  const float east = f.intensityAt(0.8f, 0.5f, 400u);
  TEST_ASSERT(east > east0);
  TEST_ASSERT(f.intensityAt(0.8f, 0.5f, 3000u) < east);  // swept past + faded
  // perpendicular to the ray (top/bottom) the sweep washes over immediately
  TEST_ASSERT(f.intensityAt(0.2f, 0.9f, 0u) > 0.95f);
}

// ------------------------------------------------- config + apply + engine
void test_config_wave_knobs_clamp() {
  Config c;
  c.roomMapping = true;
  c.waveSpeed = 99.0f;
  c.waveDecay = -3.0f;
  c.waveWidth = 0.0f;
  c.maxWaves = 99;
  clampConfig(c);
  TEST_ASSERT_TRUE(c.roomMapping);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, c.waveSpeed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, c.waveDecay);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, c.waveWidth);
  TEST_ASSERT_EQUAL_INT(12, (int)c.maxWaves);

  Config low;
  low.waveDecay = 0.0f;
  low.maxWaves = 2;
  clampConfig(low);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, low.waveDecay);
  TEST_ASSERT_EQUAL_INT(4, (int)low.maxWaves);
}

void test_apply_zone_scale_default_is_identity() {
  Config c;
  defaultConfig(c);
  Look lk;
  lk.brightness = 0.5f;
  lk.saturation = 0.7f;
  lk.hueShift = 0.1f;

  Themes::ThemeFrame a;
  a.brightness = 1.0f;
  a.saturation = 1.0f;
  a.intensity = 1.0f;
  Themes::ThemeFrame b = a;
  applyToThemeFrame(a, lk, c);           // old call shape / default arg
  applyToThemeFrame(b, lk, c, 1.0f);     // explicit pass-through scale
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.brightness, b.brightness);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.saturation, b.saturation);

  Themes::ThemeFrame dim;
  dim.brightness = 1.0f;
  dim.saturation = 1.0f;
  dim.intensity = 1.0f;
  applyToThemeFrame(dim, lk, c, 0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, b.brightness * 0.5f, dim.brightness);
  // over-scale is clamped so brightness never exceeds the apply ceiling
  Themes::ThemeFrame hot = a;
  applyToThemeFrame(hot, lk, c, 5.0f);
  TEST_ASSERT(hot.brightness <= 1.0f);
}

void test_engine_spawns_wave_on_boom_only_when_mapping() {
  // mapping OFF: video booms react but never touch the wave field
  Config off;
  defaultConfig(off);
  CinematicEngine eng;
  eng.configure(off);
  uint32_t now = 1000;
  SpatialInfo sp;
  sp.focusX = 230;
  sp.focusY = 40;
  sp.zoneCount = 1;
  sp.zones[0] = SpatialInfo::Zone{ZONE_RIGHT, 80};
  Frame boom = videoFrame(1u, now, sceneframe::SCENE_EXPLOSION,
                          sceneframe::SEVENT_BOOM, 100, 255, 30, 200, 200, 255);
  eng.update(quietAudio(), &boom, now, &sp);
  TEST_ASSERT_EQUAL_INT(0, eng.waves().activeCount());
  TEST_ASSERT_FALSE(eng.status().spatialActive);

  // mapping ON: the boom blooms at the focus point
  Config on;
  defaultConfig(on);
  on.roomMapping = true;
  CinematicEngine eng2;
  eng2.configure(on);
  eng2.update(quietAudio(), &boom, now, &sp);
  now += 16;
  eng2.update(quietAudio(), &boom, now, &sp);
  TEST_ASSERT(eng2.waves().activeCount() >= 1);
  TEST_ASSERT_TRUE(eng2.status().spatialActive);
  const float near = eng2.waves().intensityAt(0.90f, 0.16f, now);
  const float far = eng2.waves().intensityAt(0.05f, 0.84f, now);
  TEST_ASSERT(near > far);
  // and the status carries the spatial flag through the web-layer contract
  TEST_ASSERT_TRUE(eng2.status().spatialActive);
}

void test_engine_change_spawns_directional_sweep() {
  Config c;
  defaultConfig(c);
  c.roomMapping = true;
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 500;
  Frame chase = videoFrame(1u, now, sceneframe::SCENE_CHASE,
                           sceneframe::SEVENT_CHANGE, 90, 200, 40, 160, 180, 230);
  SpatialInfo left;
  left.focusX = 30;
  left.focusY = 128;
  eng.update(quietAudio(), &chase, now, &left);
  now += 300;
  SpatialInfo right;
  right.focusX = 220;
  right.focusY = 128;
  Frame chase2 = videoFrame(2u, now, sceneframe::SCENE_CHASE,
                            sceneframe::SEVENT_CHANGE, 90, 200, 40, 160, 180, 230);
  eng.update(quietAudio(), &chase2, now, &right);

  TEST_ASSERT(eng.waves().activeCount() >= 1);
  // the rightward sweep lights the east side far before the distant west
  const float east = eng.waves().intensityAt(0.86f, 0.5f, now);
  const float westFar = eng.waves().intensityAt(0.05f, 0.5f, now);
  TEST_ASSERT(east > westFar);
}

// ---------------------------------------------------------------------
// CompanionPicker — multi-source identity (spec: source-identity hardening)

cine::SourceKey skey(uint32_t ip, uint16_t port) {
  cine::SourceKey k;
  k.ip = ip;
  k.port = port;
  return k;
}

void test_picker_single_source_accepted() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(0x0A000001, 9772);  // 10.0.0.1

  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT,
                        p.accept(a, 1, 1000, 1000));
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT,
                        p.accept(a, 2, 1016, 1016));
  TEST_ASSERT_EQUAL_INT(0, p.activeIndex());
  TEST_ASSERT_EQUAL_INT(1, p.sourceCount());
  TEST_ASSERT_NOT_NULL(p.active());
  TEST_ASSERT_TRUE(p.active()->active);
  TEST_ASSERT_EQUAL_INT32(2, (int32_t)p.active()->validCount);
  TEST_ASSERT_EQUAL_UINT(1016, p.lastRxMs());
}

void test_picker_replay_and_out_of_order_rejected() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(2, 9772);

  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT, p.accept(a, 10, 1000, 1000));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_REPLAY, p.accept(a, 10, 1016, 1016));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_REPLAY, p.accept(a, 9, 1032, 1032));
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT, p.accept(a, 11, 1048, 1048));
  TEST_ASSERT_EQUAL_INT32(2, (int32_t)p.active()->validCount);

  // seq wrap across 2^32 is a legal forward step on a rolling counter
  cine::CompanionPicker w;
  w.configure(1200);
  const cine::SourceKey wsrc = skey(2, 9772);
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT,
                        w.accept(wsrc, 0xFFFFFFF0u, 2000, 2000));
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT,
                        w.accept(wsrc, 2, 2016, 2016));
  // ...but a backward step out of the wrap is still rejected
  TEST_ASSERT_EQUAL_INT(cine::REJECT_REPLAY,
                        w.accept(wsrc, 0xFFFFFFFFu, 2032, 2032));
  TEST_ASSERT_EQUAL_INT32(2, (int32_t)w.active()->validCount);
}

void test_picker_second_source_ignored_while_current_live() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(3, 9772);
  const cine::SourceKey b = skey(4, 9772);

  p.accept(a, 1, 1000, 1000);
  p.accept(a, 2, 1016, 1016);
  // B shows up mid-scene: tracked but never committed while A is live
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(b, 1, 1100, 1100));
  TEST_ASSERT_EQUAL_INT(0, p.activeIndex());
  TEST_ASSERT_TRUE(p.active()->active);
  TEST_ASSERT_EQUAL_INT(2, p.sourceCount());
  TEST_ASSERT_EQUAL_INT32(2, (int32_t)(p.active()->validCount));
  // B's own ordering is still enforced (its seq 1 was already consumed)
  TEST_ASSERT_EQUAL_INT(cine::REJECT_REPLAY, p.accept(b, 1, 1116, 1116));
}

void test_picker_switch_after_current_stale() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(5, 9772);
  const cine::SourceKey b = skey(6, 9772);

  p.accept(a, 1, 1000, 1000);   // active = A
  // A goes quiet; at t=3000 it is long stale (diff 2000 >= 1200)
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT, p.accept(b, 1, 3000, 3000));
  const cine::SourceInfo* act = p.active();
  TEST_ASSERT_NOT_NULL(act);
  TEST_ASSERT_TRUE(cine::sameKey(act->key, b));  // B now drives the engine
}

void test_picker_tie_break_votes_beat_recency() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(7, 9772);
  const cine::SourceKey b = skey(8, 9772);
  const cine::SourceKey c = skey(9, 9772);

  // A active and live until t=2216 (lastRx 1016 + stale 1200)
  p.accept(a, 1, 1000, 1000);
  p.accept(a, 2, 1016, 1016);
  // B bursts while A is live: tracked (REJECT_INACTIVE), kept, counted
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(b, 1, 1500, 1500));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(b, 2, 1516, 1516));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(b, 3, 1532, 1532));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 1, 2000, 2000));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 2, 2016, 2016));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 3, 2032, 2032));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 4, 2048, 2048));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 5, 2064, 2064));

  // A stale at t=3000. B is the sender (fresh) but C has more votes (5 vs 4)
  // and is still alive (2064 -> diff 936 < 1200): preference picks C.
  TEST_ASSERT_EQUAL_INT32((int32_t)3, (int32_t)p.info(p.indexOf(b))->validCount);
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(b, 4, 3000, 3000));
  TEST_ASSERT_EQUAL_INT32((int32_t)5, (int32_t)p.info(p.indexOf(c))->validCount);
  TEST_ASSERT_TRUE(cine::sameKey(p.active()->key, c));
}

void test_picker_table_full_eviction() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(11, 9772);
  const cine::SourceKey b = skey(12, 9772);
  const cine::SourceKey c = skey(13, 9772);
  const cine::SourceKey d = skey(14, 9772);
  const cine::SourceKey e = skey(15, 9772);

  p.accept(a, 1, 1000, 1000);
  p.accept(a, 2, 1016, 1016);
  p.accept(a, 3, 1032, 1032);
  p.accept(a, 4, 1048, 1048);  // active A, 4 votes, live until 2248
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(b, 1, 1500, 1500));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 1, 1600, 1600));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 2, 1616, 1616));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(c, 3, 1632, 1632));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(d, 1, 1700, 1700));
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(d, 2, 1716, 1716));
  TEST_ASSERT_EQUAL_INT(4, p.sourceCount());

  // 5th source while A still live (2000-1048=952 < 1200): B is the
  // least-deserving non-active keeper (1 vote, oldest) and gets evicted.
  TEST_ASSERT_EQUAL_INT(cine::REJECT_INACTIVE, p.accept(e, 1, 2000, 2000));
  TEST_ASSERT_EQUAL_INT(4, p.sourceCount());
  TEST_ASSERT_EQUAL_INT(-1, p.indexOf(b));
  TEST_ASSERT(p.indexOf(e) >= 0);
  TEST_ASSERT_TRUE(cine::sameKey(p.active()->key, a));   // A never evicted
}

void test_picker_skew_estimate() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(21, 9772);

  p.accept(a, 1, 1000, 1000);   // seed
  p.accept(a, 2, 1002, 1500);   // <1000 ms elapsed: baseline not ready
  TEST_ASSERT_EQUAL_INT32(0, p.skewPpm());
  p.accept(a, 3, 2005, 2000);   // host gained +5 ms over 1000 local => +5000 ppm
  TEST_ASSERT_INT_WITHIN(100, 5000, p.skewPpm());
  p.accept(a, 4, 3010, 3000);   // same rate, smoothed stays ~+5000
  TEST_ASSERT_INT_WITHIN(200, 5000, p.skewPpm());
}

void test_picker_switch_back_recovery() {
  cine::CompanionPicker p;
  p.configure(1200);
  const cine::SourceKey a = skey(31, 9772);
  const cine::SourceKey b = skey(32, 9772);

  p.accept(a, 1, 1000, 1000);
  p.accept(a, 2, 1016, 1016);
  // B takes over after A dies, drives a while, then dies too
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT, p.accept(b, 1, 3000, 3000));
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT, p.accept(b, 2, 3016, 3016));
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT, p.accept(b, 3, 3032, 3032));
  TEST_ASSERT_TRUE(cine::sameKey(p.active()->key, b));
  // A returns with its counter continuing: alive and best => back to A
  TEST_ASSERT_EQUAL_INT(cine::ACCEPT_COMMIT, p.accept(a, 3, 5000, 5000));
  TEST_ASSERT_TRUE(cine::sameKey(p.active()->key, a));
  TEST_ASSERT_EQUAL_INT32(3, (int32_t)p.info(p.indexOf(a))->validCount);
}

// ---------------------------------------------------------------------
// Engine: sync offset, comfort tier, link latency (spec #5)

void test_sync_offset_delays_flash_envelope() {
  Config c;
  defaultConfig(c);
  c.syncOffsetMs = 300;
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame none = videoFrame(1u, now, SCENE_ACTION, SEVENT_NONE, 100, 128, 40,
                          160, 160, 128);
  Frame flash = videoFrame(2u, now, SCENE_ACTION, SEVENT_FLASH, 100, 255, 40,
                           160, 160, 200);

  eng.update(quietAudio(), &none, now);
  now += 16;
  eng.update(quietAudio(), &flash, now);  // triggers, but schedules +300 ms
  TEST_ASSERT_EQUAL_FLOAT(0.0f, eng.look().flash);

  // while the envelope is parked the transient stays invisible
  now = 300;
  eng.update(quietAudio(), &none, now);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, eng.look().flash);

  // once the scheduled start passes the flash lands on the AV beat
  now = 316;
  eng.update(quietAudio(), &none, now);
  TEST_ASSERT(eng.look().flash > 0.0f);
}

void test_sync_offset_negative_leads_envelope() {
  Config c;
  defaultConfig(c);
  c.syncOffsetMs = -200;  // envelope began 200 ms ago => past its 180 ms hold
  CinematicEngine eng;
  eng.configure(c);

  uint32_t now = 0;
  Frame none = videoFrame(1u, now, SCENE_ACTION, SEVENT_NONE, 100, 128, 40,
                          160, 160, 128);
  Frame flash = videoFrame(2u, now, SCENE_ACTION, SEVENT_FLASH, 100, 255, 40,
                           160, 160, 200);
  eng.update(quietAudio(), &none, now);
  eng.update(quietAudio(), &flash, now);  // start = now - 200
  const float led = eng.look().flash;
  TEST_ASSERT(led > 0.0f);

  Config c0;
  defaultConfig(c0);
  CinematicEngine eng0;
  eng0.configure(c0);
  eng0.update(quietAudio(), &none, 0);
  eng0.update(quietAudio(), &flash, 0);
  const float normal = eng0.look().flash;
  TEST_ASSERT(led < normal);  // the envelope has already consumed 200 ms
}

void test_comfort_film_dims_flash_and_brightness() {
  auto run = [](int comfort) {
    Config c;
    defaultConfig(c);
    c.comfort = comfort;
    c.maxBrightness = 0.5f;
    c.flashIntensity = 1.0f;
    c.reaction = 1.0f;
    c.sensitivity = 0.6f;
    c.visualInfluence = 0.7f;
    CinematicEngine eng;
    eng.configure(c);
    uint32_t now = 0;
    Frame none = videoFrame(1u, now, SCENE_ACTION, SEVENT_NONE, 100, 128, 40,
                            160, 160, 128);
    Frame flash = videoFrame(2u, now, SCENE_ACTION, SEVENT_FLASH, 100, 255, 40,
                             160, 160, 200);
    eng.update(quietAudio(), &none, now);
    now += 16;
    eng.update(quietAudio(), &flash, now);
    return eng.look();
  };

  const Look lf = run(cine::COMFORT_FILM);
  const Look ls = run(cine::COMFORT_STANDARD);
  const Look le = run(cine::COMFORT_EXTREME);
  TEST_ASSERT(ls.flash > 0.0f);
  TEST_ASSERT(lf.flash < ls.flash);
  TEST_ASSERT(le.flash >= ls.flash);
  // brightness ceiling is capped by the comfort tier too (Film calm dims)
  TEST_ASSERT(lf.brightness <= ls.brightness);
}

void test_comfort_film_extends_flash_interval() {
  auto sustained = [](int comfort, float flashIntensity) {
    Config c;
    defaultConfig(c);
    c.comfort = comfort;
    c.flashMinGapMs = 90;
    c.flashIntensity = flashIntensity;
    c.flashDurationMs = 180;
    CinematicEngine eng;
    eng.configure(c);
    uint32_t now = 0;
    Frame none = videoFrame(1u, now, SCENE_ACTION, SEVENT_NONE, 100, 128, 40,
                            160, 160, 128);
    Frame flash = videoFrame(2u, now, SCENE_ACTION, SEVENT_FLASH, 100, 255, 40,
                             160, 160, 200);
    eng.update(quietAudio(), &none, now);
    for (int i = 0; i < 25; ++i) {  // ~400 ms of continuous flashing
      now += 16;
      eng.update(quietAudio(), &flash, now);
    }
    return eng.look().flash;
  };
  // standard re-triggers every ~90 ms so the envelope stays pinned at peak;
  // film's 2.2x gap (198 ms) lets it decay between flashes => lower impulse
  const float fs = sustained(cine::COMFORT_STANDARD, 1.0f);
  const float ff = sustained(cine::COMFORT_FILM, 1.0f);
  TEST_ASSERT(ff < fs);
}

void test_link_latency_and_jitter_estimated() {
  Config c;
  defaultConfig(c);
  CinematicEngine eng;
  eng.configure(c);

  // companion host clock advances +33 ms per +50 ms local arrival => the one
  // -way link latency is (50-33)/2 = 8 ms, repeated eight times.
  uint32_t now = 1000;
  uint32_t host = 1000;
  for (int i = 0; i < 8; ++i) {
    Frame f = videoFrame((uint32_t)(i + 1), host, SCENE_ACTION, SEVENT_NONE,
                         100, 128, 40, 160, 160, 128);
    eng.update(quietAudio(), &f, now);
    now += 50;
    host += 33;
  }
  TEST_ASSERT_INT_WITHIN(2, 5, eng.status().linkLatencyMs);
  TEST_ASSERT_INT_WITHIN(3, 0, eng.status().jitterMs);
}

// ---------------------------------------------------------------------
// TestInjector — synthetic SceneFrame source (spec: test/demo mode)

void test_injector_runs_entries_and_stops() {
  cine::TestEntry a, b;
  a.lengthMs = 100;
  a.scene = SCENE_EXPLOSION;
  a.event = SEVENT_BOOM;
  b.lengthMs = 200;
  b.scene = SCENE_SPEECH;
  b.event = SEVENT_NONE;
  cine::TestEntry script[2] = {a, b};
  cine::TestInjector inj;
  inj.start(script, 2, false, 1000);

  Frame f;
  SpatialInfo sp;
  bool hs = false;
  TEST_ASSERT_TRUE(inj.step(1050, f, sp, hs));
  TEST_ASSERT_EQUAL_UINT8(SCENE_EXPLOSION, f.sceneId);
  TEST_ASSERT_EQUAL_UINT8(SEVENT_BOOM, f.eventId);

  TEST_ASSERT_TRUE(inj.step(1110, f, sp, hs));
  TEST_ASSERT_EQUAL_UINT8(SCENE_SPEECH, f.sceneId);

  // cumulative dwell (300 ms) spent => non-looping run deactivates
  TEST_ASSERT_FALSE(inj.step(1500, f, sp, hs));
  TEST_ASSERT_FALSE(inj.active());
}

void test_injector_loops_and_seq_monotonic() {
  cine::TestEntry e;
  e.lengthMs = 0;  // zero-dwell entry covers the whole (0-length) loop
  e.scene = SCENE_MUSIC;
  cine::TestInjector inj;
  inj.start(&e, 1, true, 0);
  Frame f;
  SpatialInfo sp;
  bool hs = false;
  uint32_t prev = 0;
  bool first = true;
  for (uint32_t t = 16; t <= 1000; t += 16) {
    TEST_ASSERT_TRUE(inj.step(t, f, sp, hs));
    if (!first) TEST_ASSERT(f.seq > prev);
    first = false;
    prev = f.seq;
  }
  TEST_ASSERT_TRUE(inj.active());
  TEST_ASSERT_EQUAL_UINT8(SCENE_MUSIC, f.sceneId);
}

void test_injector_spatial_sample() {
  cine::TestEntry e;
  e.lengthMs = 500;
  e.focusX = 40;
  e.focusY = 128;
  cine::TestInjector inj;
  inj.start(&e, 1, false, 100);
  Frame f;
  SpatialInfo sp;
  bool hs = false;
  TEST_ASSERT_TRUE(inj.step(200, f, sp, hs));
  TEST_ASSERT_TRUE(hs);
  TEST_ASSERT_EQUAL_UINT8(40u, sp.focusX);
  TEST_ASSERT_EQUAL_UINT8(128u, sp.focusY);

  e.focusX = -1;
  e.focusY = -1;
  inj.start(&e, 1, false, 100);
  TEST_ASSERT_TRUE(inj.step(200, f, sp, hs));
  TEST_ASSERT_FALSE(hs);
}

}  // namespace

// --------------------------------------------------------------------- main
int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_codec_roundtrip);
  RUN_TEST(test_codec_rejects_bad_input);
  RUN_TEST(test_codec_idents);
  RUN_TEST(test_presets_tune_knobs_and_keep_network);
  RUN_TEST(test_clamp_normalizes_garbage);
  RUN_TEST(test_analyzer_boom_detection_and_cooldown);
  RUN_TEST(test_analyzer_sustained_loud_gates_booms);
  RUN_TEST(test_analyzer_whisper_and_silence);
  RUN_TEST(test_analyzer_dialogue);
  RUN_TEST(test_analyzer_tension_builds_and_collapses);
  RUN_TEST(test_engine_audio_only_fallback);
  RUN_TEST(test_engine_video_primary_fusion);
  RUN_TEST(test_engine_flash_envelope_and_min_gap);
  RUN_TEST(test_engine_boom_event_cooldown);
  RUN_TEST(test_engine_sustained_music_pulses_but_loud_quiet);
  RUN_TEST(test_engine_failsafe_when_feed_goes_stale);
  RUN_TEST(test_engine_whisper_dims_and_dark_floor);
  RUN_TEST(test_engine_video_colour_tint);
  RUN_TEST(test_engine_genre_horror_dims);
  RUN_TEST(test_engine_source_enum_ordering);
  RUN_TEST(test_apply_to_theme_frame);
  RUN_TEST(test_memory_labels_and_genre_auto);
  RUN_TEST(test_memory_hysteresis_switches_after_repeated_votes);
  RUN_TEST(test_memory_change_event_bypasses_hysteresis);
  RUN_TEST(test_memory_dwell_and_transition_timers);
  RUN_TEST(test_memory_energy_rises_with_events_and_decays);
  RUN_TEST(test_memory_ring_bounded_and_coalesced);
  RUN_TEST(test_director_mood_cascade);
  RUN_TEST(test_director_hold_prevents_flicker);
  RUN_TEST(test_director_genre_auto_stays_bounded);
  RUN_TEST(test_director_flash_scale_never_exceeds_one);
  RUN_TEST(test_engine_status_exposes_intent);
  RUN_TEST(test_spatial_block_roundtrip);
  RUN_TEST(test_spatial_block_rides_an_overlong_frame);
  RUN_TEST(test_spatial_block_rejects_malformed);
  RUN_TEST(test_spatial_zone_idents);
  RUN_TEST(test_wave_rest_is_zero_and_peaks_at_spawn);
  RUN_TEST(test_wave_propagates_away_then_fades);
  RUN_TEST(test_wave_ring_is_bounded_and_overwrites);
  RUN_TEST(test_wave_line_sweep_lights_along_its_path);
  RUN_TEST(test_config_wave_knobs_clamp);
  RUN_TEST(test_apply_zone_scale_default_is_identity);
  RUN_TEST(test_engine_spawns_wave_on_boom_only_when_mapping);
  RUN_TEST(test_engine_change_spawns_directional_sweep);
  RUN_TEST(test_picker_single_source_accepted);
  RUN_TEST(test_picker_replay_and_out_of_order_rejected);
  RUN_TEST(test_picker_second_source_ignored_while_current_live);
  RUN_TEST(test_picker_switch_after_current_stale);
  RUN_TEST(test_picker_tie_break_votes_beat_recency);
  RUN_TEST(test_picker_table_full_eviction);
  RUN_TEST(test_picker_skew_estimate);
  RUN_TEST(test_picker_switch_back_recovery);
  RUN_TEST(test_sync_offset_delays_flash_envelope);
  RUN_TEST(test_sync_offset_negative_leads_envelope);
  RUN_TEST(test_comfort_film_dims_flash_and_brightness);
  RUN_TEST(test_comfort_film_extends_flash_interval);
  RUN_TEST(test_link_latency_and_jitter_estimated);
  RUN_TEST(test_injector_runs_entries_and_stops);
  RUN_TEST(test_injector_loops_and_seq_monotonic);
  RUN_TEST(test_injector_spatial_sample);
  return UNITY_END();
}