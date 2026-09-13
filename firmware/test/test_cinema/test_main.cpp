#include <unity.h>

#include "audio/AudioFrame.h"
#include "cinema/CinematicApply.h"
#include "cinema/CinematicConfig.h"
#include "cinema/CinematicDirector.h"
#include "cinema/CinematicEngine.h"
#include "cinema/SceneAnalyzer.h"
#include "cinema/SceneFrame.h"
#include "cinema/SceneMemory.h"

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
  return UNITY_END();
}