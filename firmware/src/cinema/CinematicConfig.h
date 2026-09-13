#pragma once
#include <stdint.h>
#include <string.h>

// Cinematic Mode configuration (spec §20-§23). Pure struct + bounded helpers so
// the same code is shared by the firmware, the host tests and the web/mock
// API. Lives in its own header (included by Config.h) to keep it out of the
// config JSON layer's dependency chain.

namespace cine {

// Reaction presets (spec §20). Balanced is the shipped default.
enum Mode : int {
  MODE_SUBTLE = 0,
  MODE_BALANCED,
  MODE_IMMERSIVE,
  MODE_DYNAMIC,
  MODE_EXTREME,
  MODE_COUNT,
};

// Genre overlays (spec §21) — thin behaviour + tint tuning on top of a mode.
// GENRE_AUTO lets the Director pick the overlay from scene memory + audio
// (tension-heavy content behaves horror-like, high-energy content anime-like,
// everything else neutral). It only biases pacing/flash/pulse — see
// CinematicDirector.
enum Genre : int {
  GENRE_NONE = 0,
  GENRE_HORROR,
  GENRE_ANIME,
  GENRE_AUTO,
  GENRE_COUNT,
};

struct Config {
  bool      enabled = false;       // master on/off for the whole mode
  int       mode = MODE_BALANCED;
  int       genre = GENRE_NONE;

  // reaction
  float     sensitivity = 1.0f;    // 0.25..3 input gain multiplier
  float     reaction = 0.8f;       // 0..1 master reaction strength
  float     visualInfluence = 0.7f;// how much the SceneFrame drives the look
  float     audioInfluence = 0.5f; // how much on-board audio drives the look
  float     colorInfluence = 1.0f; // 0..2 dominant-colour pull strength

  // behaviour
  float     speed = 0.5f;          // 0..1 motion/movement boost
  float     smoothing = 0.4f;      // 0..1 (1 = slow/sticky)
  float     flashIntensity = 0.6f; // 0..1 flash ceiling
  uint16_t  flashDurationMs = 180; // hold time of a flash envelope
  uint16_t  flashMinGapMs = 90;    // hard cap on flash frequency (no strobe)
  uint16_t  boomCooldownMs = 450;  // min gap between boom/impact envelopes
  float     whisperDim = 0.55f;    // brightness scale during whisper moments
  float     maxBrightness = 1.0f;  // 0..1 ceiling on modulated brightness
  float     ambientFloor = 0.06f;  // min brightness in near-black scenes

  // spatial room mapping (spec: Spatial Wave Propagation). OFF by default so
  // nothing changes until the user both enables it and places their strips.
  bool      roomMapping = false;   // map event focus into per-strip waves
  float     waveSpeed = 2.0f;      // 0.5..5 room units / second
  float     waveDecay = 0.8f;      // 0.1..2 per-second fading of wave energy
  float     waveWidth = 0.7f;      // 0.1..1 wavefront bulge width
  uint8_t   maxWaves = 8;          // 4..12 bounded wave ring depth

  // companion (SceneLink) network
  bool      receiveUdp = false;    // listen for companion SceneFrames
  char      group[16] = "239.255.42.11";
  uint16_t  port = 9772;
  uint16_t  staleMs = 1200;        // companion silent => pure audio fallback
};

inline void clampConfig(Config& c) {
  if (c.mode < 0) c.mode = MODE_SUBTLE;
  if (c.mode >= MODE_COUNT) c.mode = MODE_COUNT - 1;
  if (c.genre < 0) c.genre = GENRE_NONE;
  if (c.genre >= GENRE_COUNT) c.genre = GENRE_COUNT - 1;

  auto cl = [](float& v, float lo, float hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    if (v != v) v = lo;  // NaN guard
  };
  cl(c.sensitivity, 0.25f, 3.0f);
  cl(c.reaction, 0.0f, 1.5f);   // Extreme preset runs hot by design (1.25)
  cl(c.visualInfluence, 0.0f, 1.0f);
  cl(c.audioInfluence, 0.0f, 1.0f);
  cl(c.colorInfluence, 0.0f, 2.0f);
  cl(c.speed, 0.0f, 1.0f);
  cl(c.smoothing, 0.0f, 1.0f);
  cl(c.flashIntensity, 0.0f, 1.0f);
  cl(c.whisperDim, 0.0f, 1.0f);
  cl(c.maxBrightness, 0.0f, 1.0f);
  cl(c.ambientFloor, 0.0f, 0.5f);
  cl(c.waveSpeed, 0.5f, 5.0f);
  cl(c.waveDecay, 0.1f, 2.0f);
  cl(c.waveWidth, 0.1f, 1.0f);

  auto clampU8 = [](uint8_t& v, uint8_t lo, uint8_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
  };
  clampU8(c.maxWaves, 4, 12);

  auto clampU16 = [](uint16_t& v, uint16_t lo, uint16_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
  };
  clampU16(c.flashDurationMs, 30, 1000);
  clampU16(c.flashMinGapMs, 20, 1000);
  clampU16(c.boomCooldownMs, 50, 2000);
  clampU16(c.port, 1024, 65535);
  clampU16(c.staleMs, 100, 10000);
  c.group[sizeof(c.group) - 1] = '\0';
}

inline void defaultConfig(Config& c) {
  c = Config();
  clampConfig(c);
}

// Fills the scalar knobs for a preset (mode). Network + hardware fields are
// preserved so the user can pick a preset without losing the companion port.
inline void applyPreset(Config& c, int mode) {
  c.mode = mode;
  switch (mode) {
    case MODE_SUBTLE:
      c.reaction = 0.45f;
      c.visualInfluence = 0.5f;
      c.audioInfluence = 0.3f;
      c.colorInfluence = 0.8f;
      c.speed = 0.3f;
      c.flashIntensity = 0.35f;
      c.whisperDim = 0.35f;
      c.smoothing = 0.6f;
      c.flashDurationMs = 200;
      c.flashMinGapMs = 120;
      c.boomCooldownMs = 600;
      break;
    case MODE_IMMERSIVE:
      c.reaction = 0.95f;
      c.visualInfluence = 0.85f;
      c.audioInfluence = 0.65f;
      c.colorInfluence = 1.1f;
      c.speed = 0.65f;
      c.flashIntensity = 0.75f;
      c.whisperDim = 0.7f;
      c.smoothing = 0.45f;
      c.flashDurationMs = 220;
      c.flashMinGapMs = 110;
      c.boomCooldownMs = 500;
      break;
    case MODE_DYNAMIC:
      c.reaction = 1.0f;
      c.visualInfluence = 0.8f;
      c.audioInfluence = 0.85f;
      c.colorInfluence = 1.2f;
      c.speed = 0.85f;
      c.flashIntensity = 0.9f;
      c.whisperDim = 0.8f;
      c.smoothing = 0.3f;
      c.flashDurationMs = 200;
      c.flashMinGapMs = 80;
      c.boomCooldownMs = 400;
      break;
    case MODE_EXTREME:
      c.reaction = 1.25f;
      c.visualInfluence = 0.9f;
      c.audioInfluence = 1.0f;
      c.colorInfluence = 1.3f;
      c.speed = 1.0f;
      c.flashIntensity = 1.0f;
      c.whisperDim = 0.9f;
      c.smoothing = 0.25f;
      c.flashDurationMs = 240;
      c.flashMinGapMs = 60;
      c.boomCooldownMs = 320;
      break;
    case MODE_BALANCED:
    default:
      c.reaction = 0.8f;
      c.visualInfluence = 0.7f;
      c.audioInfluence = 0.5f;
      c.colorInfluence = 1.0f;
      c.speed = 0.5f;
      c.flashIntensity = 0.6f;
      c.whisperDim = 0.55f;
      c.smoothing = 0.4f;
      c.flashDurationMs = 180;
      c.flashMinGapMs = 90;
      c.boomCooldownMs = 450;
      break;
  }
  clampConfig(c);
}

inline const char* modeIdent(int m) {
  switch (m) {
    case MODE_SUBTLE: return "subtle";
    case MODE_IMMERSIVE: return "immersive";
    case MODE_DYNAMIC: return "dynamic";
    case MODE_EXTREME: return "extreme";
    default: return "balanced";
  }
}

inline const char* modeLabel(int m) {
  switch (m) {
    case MODE_SUBTLE: return "Gentle";
    case MODE_IMMERSIVE: return "Immersive";
    case MODE_DYNAMIC: return "Dynamic";
    case MODE_EXTREME: return "Extreme";
    default: return "Balanced";
  }
}

inline const char* genreIdent(int g) {
  switch (g) {
    case GENRE_HORROR: return "horror";
    case GENRE_ANIME: return "anime";
    case GENRE_AUTO: return "auto";
    default: return "none";
  }
}

inline const char* genreLabel(int g) {
  switch (g) {
    case GENRE_HORROR: return "Horror";
    case GENRE_ANIME: return "Anime";
    case GENRE_AUTO: return "Automatic";
    default: return "None";
  }
}

}  // namespace cine