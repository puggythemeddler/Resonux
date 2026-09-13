#pragma once
#include "cinema/CinematicConfig.h"
#include "cinema/CinematicDirector.h"
#include "cinema/SceneAnalyzer.h"
#include "cinema/SceneFrame.h"
#include "cinema/SceneMemory.h"
#include "cinema/SpatialBlock.h"
#include "cinema/SpatialWaveField.h"
#include "util/Rgb.h"
#include <stdint.h>
#include <math.h>

// Cinematic fusion engine (spec §12-§19, §21-§23). Pure C++ / Arduino-free so
// the host tests exercise the whole behaviour: scene + event envelopes, audio
// cross-checks, cooldowns (no strobe, no boom-spam on loud music), whisper /
// tension / dark handling, video dominant-colour blending and the failsafe
// path back to audio-only when the companion feed goes stale.
//
// The engine never touches hardware: it produces a `Look` which the caller
// applies to the ThemeEngine output (spec §36). Hardware is driven exclusively
// by ThemeEngine -> Effects -> LEDDriver.

namespace cine {

enum EngineSource : uint8_t {
  SRC_LOCAL_AUDIO = 0,  // companion not fresh: pure on-board audio
  SRC_VIDEO,            // companion feed driving, local audio idle/quiet
  SRC_FUSED,            // both live — video events primary, audio cross-check
};

struct Status {
  EngineSource source = SRC_LOCAL_AUDIO;
  sceneframe::SceneKind scene = sceneframe::SCENE_UNDEFINED;
  sceneframe::SceneEvent event = sceneframe::SEVENT_NONE;
  uint8_t eventConfidence = 0;
  uint8_t sceneConfidence = 0;
  uint8_t luminance = 0;   // smoothed scene average (0..255)
  uint8_t motion = 0;      // smoothed motion energy (0..255)
  uint8_t progAudio = 0;   // companion program-audio level (0..255)
  uint8_t hue = 0;         // smoothed dominant hue (0..255)
  uint8_t sat = 0;
  uint8_t val = 0;
  float audioLevel = 0.0f;
  float boom = 0.0f;
  float tension = 0.0f;
  uint32_t lastFrameMs = 0;  // local time of the last accepted SceneFrame
  bool companionAlive = false;
  // intent layer (spec §17)
  Mood mood = MOOD_CALM;
  float moodEnergy = 0.0f;
  uint8_t recentEvents = 0;
  // spatial layer (spec: room mapping)
  bool spatialActive = false;  // roomMapping on AND a fresh spatial sample
};

// Modulation target, produced once per LED frame and handed to the theme
// output via applyToThemeFrame().
struct Look {
  float brightness = 1.0f;    // 0..1 multiplier (already kept <= maxBrightness)
  float saturation = 1.0f;    // 0..1
  float hueShift = 0.0f;      // 0..1 hue offset added to colourShift
  float movement = 0.0f;      // 0..1 motion-speed boost
  float pulse = 0.0f;         // 0..1 beat-pulse depth
  float flash = 0.0f;         // 0..1 transient flash energy
  float impact = 0.0f;        // 0..1 boom slam envelope
  float calm = 0.0f;          // 0..1 whisper/quiet factor
  float tintMix = 0.0f;       // 0..1 blend toward video dominant colour
  Rgb   tint;                 // dominant colour to blend toward
  EngineSource source = SRC_LOCAL_AUDIO;
  sceneframe::SceneKind scene = sceneframe::SCENE_UNDEFINED;
  sceneframe::SceneEvent event = sceneframe::SEVENT_NONE;
  uint8_t confidence = 0;
};

inline float cl01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

// circular hue lerp (hues live on a ring)
inline float lerpHue(float a, float b, float t) {
  float d = b - a;
  if (d > 0.5f) d -= 1.0f;
  else if (d < -0.5f) d += 1.0f;
  float v = a + d * t;
  if (v < 0.0f) v += 1.0f;
  else if (v >= 1.0f) v -= 1.0f;
  return v;
}

class CinematicEngine {
 public:
  void configure(const Config& c) {
    _cfg = c;
    clampConfig(_cfg);
    _waveField.setMaxWaves((int)_cfg.maxWaves);
  }

  // Feed once per LED frame: local audio features from SceneAnalyzer, plus the
  // freshest SceneFrame (nullptr when the companion is absent/stale — the
  // caller checks staleness; the engine also decays into failsafe on its own).
  // `spatial` is the optional length-tagged spatial block parsed off the same
  // SceneFrame payload; the engine keeps the last good sample even if a later
  // Frame has none (the focus just persists).
  Status update(const AudioFeatures& audio,
                const sceneframe::Frame* scene, uint32_t nowMs,
                const sceneframe::SpatialInfo* spatial = nullptr);

  const Look& look() const { return _look; }
  const Status& status() const { return _status; }
  const SpatialWaveField& waves() const { return _waveField; }
  const sceneframe::SpatialInfo& spatialInfo() const { return _spatial; }
  bool hasSpatial() const { return _haveSpatial; }

 private:
  void buildLook(const AudioFeatures& audio, bool videoActive,
                 EngineSource src, sceneframe::SceneKind sceneKind,
                 uint8_t conf, uint32_t dt);

  Config _cfg;
  Look _look;
  Status _status;

  bool _haveFrame = false;
  sceneframe::Frame _last;
  uint32_t _lastFrameMs = 0;
  uint32_t _lastUpdateMs = 0;

  // smoothed video fields
  float _vHue = 0.0f, _vSat = 0.0f, _vVal = 0.0f, _vLum = 0.0f, _vMotion = 0.0f;

  // envelopes
  float _flash = 0.0f;
  uint32_t _flashHoldMs = 0;
  float _boom = 0.0f;
  uint32_t _boomHoldMs = 0;
  float _calm = 0.0f;
  float _dark = 0.0f;
  uint32_t _lastFlashStartMs = 0;
  uint32_t _lastBoomStartMs = 0;

  // scene memory + intent (spec §1, §17)
  SceneMemory _memory;
  CinematicDirector _director;
  CinematicIntent _intent;

  // spatial wave field + last sample (spec: room mapping)
  SpatialWaveField _waveField;
  sceneframe::SpatialInfo _spatial;
  bool _haveSpatial = false;
  uint32_t _lastChaseWaveMs = 0;
  uint8_t _prevFocusX = 128;
  uint8_t _prevFocusY = 128;
};

inline Status CinematicEngine::update(const AudioFeatures& audio,
                                      const sceneframe::Frame* scene,
                                      uint32_t nowMs,
                                      const sceneframe::SpatialInfo* spatial) {
  if (spatial) {
    _spatial = *spatial;
    _haveSpatial = true;
  }
  uint32_t dt = 16;
  if (_lastUpdateMs && nowMs >= _lastUpdateMs &&
      nowMs - _lastUpdateMs < 5000u) {
    dt = nowMs - _lastUpdateMs;
  }
  _lastUpdateMs = nowMs;

  if (scene) {
    _last = *scene;
    _haveFrame = true;
    _lastFrameMs = nowMs;
  }

  const uint32_t stale = _cfg.staleMs;
  const bool videoActive =
      _haveFrame && nowMs - _lastFrameMs <= stale && stale > 0;

  // fully drop a long-dead feed so the UI/status clearly shows audio fallback
  if (_haveFrame && !videoActive &&
      nowMs - _lastFrameMs > (uint32_t)stale * 4u) {
    _haveFrame = false;
  }

  EngineSource src = SRC_LOCAL_AUDIO;
  sceneframe::SceneKind sceneKind = sceneframe::SCENE_UNDEFINED;
  sceneframe::SceneEvent ev = sceneframe::SEVENT_NONE;
  uint8_t conf = 0;

  if (videoActive) {
    sceneKind = (sceneframe::SceneKind)_last.sceneId;
    ev = (sceneframe::SceneEvent)_last.eventId;
    conf = _last.eventConfidence > 100 ? 100 : _last.eventConfidence;
    src = (audio.level > 0.12f) ? SRC_FUSED : SRC_VIDEO;
  }

  const float sens = _cfg.sensitivity;

  // spatial focus for the wave field: last known sample, or room centre
  const float fx = _haveSpatial ? (float)_spatial.focusX / 255.0f : 0.5f;
  const float fy = _haveSpatial ? (float)_spatial.focusY / 255.0f : 0.5f;
  const bool mapping = _cfg.roomMapping;

  // cooldown helpers: a zero timestamp means "never fired" (sentinel drives the
  // gap to infinity so the first event always passes)
  const uint32_t sinceFlash = _lastFlashStartMs == 0
                                  ? 0xFFFFFFFFu
                                  : nowMs - _lastFlashStartMs;
  const uint32_t sinceBoom =
      _lastBoomStartMs == 0 ? 0xFFFFFFFFu : nowMs - _lastBoomStartMs;

  // ---- react to video events ----------------------------------------------
  if (videoActive && ev != sceneframe::SEVENT_NONE && conf > 0) {
    const float weight = (float)conf / 100.0f * _cfg.reaction * sens;
    switch (ev) {
      case sceneframe::SEVENT_BOOM:
        if (sinceBoom >= _cfg.boomCooldownMs) {
          _lastBoomStartMs = nowMs;
          _flash = _cfg.flashIntensity * weight;
          _flashHoldMs = _cfg.flashDurationMs;
          _boom = weight;
          _boomHoldMs = _cfg.flashDurationMs + 60u;
          if (mapping) {
            _waveField.spawn(fx, fy, cl01(weight), _cfg.waveSpeed,
                             _cfg.waveDecay, _cfg.waveWidth, nowMs);
          }
        }
        break;
      case sceneframe::SEVENT_FLASH:
        if (sinceFlash >= _cfg.flashMinGapMs) {
          _lastFlashStartMs = nowMs;
          _flash = _cfg.flashIntensity * weight;
          _flashHoldMs = _cfg.flashDurationMs;
          if (mapping) {
            _waveField.spawn(fx, fy, cl01(weight) * 0.7f, _cfg.waveSpeed,
                             _cfg.waveDecay, _cfg.waveWidth, nowMs);
          }
        }
        break;
      case sceneframe::SEVENT_CHANGE:
        if (mapping) {
          // focus moved => the light sweeps across the room; else a bloom
          if (_haveSpatial &&
              (_prevFocusX != _spatial.focusX ||
               _prevFocusY != _spatial.focusY)) {
            float dx = ((float)_spatial.focusX - (float)_prevFocusX) / 255.0f;
            float dy = ((float)_spatial.focusY - (float)_prevFocusY) / 255.0f;
            if (dx == 0.0f) dx = 1e-4f;
            _waveField.spawnLine(fx, fy, dx, dy, cl01(weight) * 0.8f,
                                 _cfg.waveSpeed, _cfg.waveDecay,
                                 _cfg.waveWidth, nowMs);
          } else {
            _waveField.spawn(fx, fy, cl01(weight) * 0.8f, _cfg.waveSpeed,
                             _cfg.waveDecay, _cfg.waveWidth, nowMs);
          }
          _prevFocusX = _spatial.focusX;
          _prevFocusY = _spatial.focusY;
        }
        break;
      case sceneframe::SEVENT_WHISPER:
        _calm += 0.012f * dt * weight;
        break;
      case sceneframe::SEVENT_DARK:
        _dark += 0.020f * dt * weight;
        break;
      default:
        break;
    }
  }

  // sustained chase action keeps feeding the wave field at the focus so the
  // room "locks on" to a panning subject, not just single-frame events
  if (videoActive && sceneKind == sceneframe::SCENE_CHASE && mapping) {
    if (nowMs - _lastChaseWaveMs >= 400u) {
      _lastChaseWaveMs = nowMs;
      _waveField.spawn(fx, fy, cl01(_vMotion) * 0.35f + 0.25f,
                       _cfg.waveSpeed, _cfg.waveDecay, _cfg.waveWidth, nowMs);
    }
  }

  // ---- react to local audio -----------------------------------------------  // sustained-loud music pulses but must not re-trigger as booms
  const float aw = _cfg.audioInfluence * sens;
  if (audio.boom > 0.01f && sinceBoom >= _cfg.boomCooldownMs) {
    _lastBoomStartMs = nowMs;
    _flash = _cfg.flashIntensity * aw * audio.boom * 0.8f;
    _flashHoldMs = _cfg.flashDurationMs;
    _boom = audio.boom * aw;
    _boomHoldMs = _cfg.flashDurationMs + 60u;
    if (mapping) {
      _waveField.spawn(fx, fy, cl01(aw * audio.boom) * 0.8f, _cfg.waveSpeed,
                       _cfg.waveDecay, _cfg.waveWidth, nowMs);
    }
  } else if (audio.impact > 0.01f && sinceFlash >= _cfg.flashMinGapMs) {
    _lastFlashStartMs = nowMs;
    _flash = _cfg.flashIntensity * aw * audio.impact * 0.6f;
    _flashHoldMs = (uint16_t)(_cfg.flashDurationMs * 0.7f);
  }
  if (audio.whisper) _calm += 0.010f * dt * aw;
  if (audio.silence) {
    _calm += 0.006f * dt;
    _dark += 0.004f * dt;
  }

  // ---- envelope decay ------------------------------------------------------
  if (_flashHoldMs) {
    if (_flashHoldMs > dt) _flashHoldMs -= dt;
    else _flashHoldMs = 0;
  } else {
    _flash *= (1.0f - 0.020f * (dt / 16.0f));
  }
  if (_boomHoldMs) {
    if (_boomHoldMs > dt) _boomHoldMs -= dt;
    else _boomHoldMs = 0;
  } else {
    _boom *= (1.0f - 0.035f * (dt / 16.0f));
  }
  _calm *= (1.0f - 0.004f * (dt / 16.0f));
  _dark *= (1.0f - 0.010f * (dt / 16.0f));
  if (_flash < 0.01f) _flash = 0.0f;
  if (_boom < 0.01f) _boom = 0.0f;
  if (_calm < 0.0f) _calm = 0.0f;
  if (_dark < 0.0f) _dark = 0.0f;

  // ---- smooth the video fields ---------------------------------------------
  if (videoActive) {
    const float kS = 0.25f * (dt / 16.0f);
    _vHue = lerpHue(_vHue, (float)_last.hue / 255.0f, kS);
    _vSat += (((float)_last.sat / 255.0f) - _vSat) * kS;
    _vVal += (((float)_last.val / 255.0f) - _vVal) * kS;
    _vLum += (((float)_last.avgLuminance / 255.0f) - _vLum) * kS;
    _vMotion += (((float)_last.motion / 255.0f) - _vMotion) * kS;
  }

  _memory.feed(videoActive ? sceneKind : sceneframe::SCENE_UNDEFINED,
               videoActive ? ev : sceneframe::SEVENT_NONE,
               videoActive ? conf : 0,
               videoActive ? _last.avgLuminance : 0, nowMs);
  SceneContext mctx = _memory.context();
  _intent = _director.compute(mctx, audio, videoActive ? &_last : nullptr,
                              _cfg, nowMs);

  buildLook(audio, videoActive, src, sceneKind, conf, dt);

  // ---- status --------------------------------------------------------------
  _status.source = src;
  _status.scene = sceneKind;
  _status.event = ev;
  _status.eventConfidence = conf;
  _status.sceneConfidence = videoActive ? conf : 0;
  _status.mood = _intent.mood;
  _status.moodEnergy = _intent.energy;
  _status.recentEvents = mctx.recentEventCount;
  _status.luminance = (uint8_t)(cl01(_vLum) * 255.0f);
  _status.motion = (uint8_t)(cl01(_vMotion) * 255.0f);
  _status.progAudio = videoActive ? _last.progAudio : 0;
  _status.hue = (uint8_t)(cl01(_vHue) * 255.0f);
  _status.sat = (uint8_t)(cl01(_vSat) * 255.0f);
  _status.val = (uint8_t)(cl01(_vVal) * 255.0f);
  _status.audioLevel = cl01(audio.level);
  _status.boom = cl01(audio.boom);
  _status.tension = cl01(audio.tension);
  _status.lastFrameMs = _lastFrameMs;
  _status.companionAlive = videoActive;
  _status.spatialActive = mapping && videoActive && _haveSpatial;
  return _status;
}

inline void CinematicEngine::buildLook(const AudioFeatures& audio,
                                       bool videoActive, EngineSource src,
                                       sceneframe::SceneKind sceneKind,
                                       uint8_t conf, uint32_t dt) {
  (void)dt;
  Look lk;
  lk.source = src;
  lk.scene = sceneKind;
  lk.event = _status.event;
  lk.confidence = conf;

  float brightness = _cfg.maxBrightness;
  float saturation = 1.0f;
  float hueShift = 0.0f;
  float movement = _cfg.speed * (0.35f + 0.65f * (videoActive ? _vMotion : 0.0f));
  float pulse = 0.0f;
  float calm = _calm;
  float dark = _dark;
  float tension = cl01(audio.tension) * _cfg.audioInfluence;

  // audio energy raises the motion floor; beats add pulse depth
  const float level = cl01(audio.level) * _cfg.audioInfluence;
  movement = cl01(movement + level * 0.35f);
  if (audio.beat) pulse = cl01(pulse + audio.beatStrength * 0.35f * _cfg.audioInfluence);
  if (level > 0.0f) pulse = cl01(pulse + level * 0.10f);
  // Director pacing: mood-appropriate pulse depth (never below zero)
  pulse *= _intent.pulseDepth;

  // Director warmth nudge: a small hue lean toward the mood's warm/cool target
  // (always clamped, only when a scene is actually present)
  if (videoActive && sceneKind != sceneframe::SCENE_UNDEFINED) {
    const float warmTarget = 0.66f - 0.59f * _intent.tintWarmth;
    hueShift = lerpHue(hueShift, warmTarget, 0.04f);
  }

  // video dominant colour -> tint + gentle hue lean
  float tintMix = 0.0f;
  Rgb tint{};
  if (videoActive && _cfg.visualInfluence > 0.01f) {
    const float ci = _cfg.colorInfluence;
    const float vc = (float)conf / 100.0f;
    tintMix = cl01(_cfg.visualInfluence * vc * 0.9f);
    tint = hslToRgb(_vHue, cl01(_vSat * 0.8f + 0.2f),
                    cl01(_vVal * 0.55f + 0.15f));
    switch (sceneKind) {
      case sceneframe::SCENE_EXPLOSION:
        tint = blend(tint, Rgb{255, 140, 40}, 0.35f);
        break;
      case sceneframe::SCENE_CHASE:
        tint = blend(tint, Rgb{255, 90, 60}, 0.25f);
        break;
      case sceneframe::SCENE_MUSIC:
        tint = blend(tint, Rgb{255, 255, 60}, 0.12f);
        break;
      default:
        break;
    }
    if (ci > 1.0f) {  // extra colour influence leans the hue as well
      hueShift = lerpHue(hueShift, _vHue, cl01(ci - 1.0f) * 0.6f);
    }
  }

  // whisper: dim + cool + desaturate
  if (calm > 0.01f) {
    brightness *= (1.0f - calm * _cfg.whisperDim * 0.75f);
    hueShift = lerpHue(hueShift, 0.66f, calm * 0.25f);
    saturation *= (1.0f - calm * 0.4f);
  }
  // dark: push down but keep the ambient floor
  if (dark > 0.01f) brightness *= (1.0f - dark * 0.55f);

  // tension: horror stingers creep instead of bounce
  if (tension > 0.01f) {
    brightness *= (1.0f - tension * 0.30f);
    hueShift = lerpHue(hueShift, 0.02f, cl01(tension * 0.35f));
    saturation *= (1.0f + tension * 0.25f);
    movement *= (1.0f - tension * 0.40f);
  }

  // flash: brightness rides to the ceiling, then the envelope decays.
  // The Director's flashScale suppresses it in calm/suspense moods; it never
  // exceeds 1.0 so the ceiling bound is preserved.
  if (_flash > 0.01f) {
    const float fs = _flash * _intent.flashScale;
    brightness = brightness + (_cfg.maxBrightness - brightness) * cl01(fs * 0.9f);
    saturation *= (1.0f - fs * 0.15f);
  }
  // boom: split-second dip before the flash for contrast
  if (_boom > 0.01f) brightness *= (1.0f - _boom * 0.12f);

  // ambient floor: near-black scenes stay subtly lit; video luminance informs it
  float floor = _cfg.ambientFloor;
  if (videoActive) {
    floor = floor > _vLum * _cfg.visualInfluence * 0.28f
                ? floor
                : _vLum * _cfg.visualInfluence * 0.28f;
  }
  if (brightness < floor) brightness = floor;
  if (brightness > _cfg.maxBrightness) brightness = _cfg.maxBrightness;

  // genre overlays (spec §21)
  switch (_cfg.genre) {
    case GENRE_HORROR:
      brightness *= 0.88f;
      saturation *= 0.9f;
      movement *= 0.85f;
      hueShift = lerpHue(hueShift, 0.02f, 0.5f);
      break;
    case GENRE_ANIME:
      brightness = brightness * 1.08f;
      saturation = saturation * 1.15f;
      movement = movement * 1.35f;
      break;
    default:
      break;
  }

  lk.brightness = cl01(brightness);
  lk.saturation = cl01(saturation);
  lk.hueShift = cl01(hueShift);
  lk.movement = cl01(movement);
  lk.pulse = cl01(pulse);
  lk.flash = cl01(_flash * _intent.flashScale);
  lk.impact = cl01(_boom);
  lk.calm = cl01(calm);
  lk.tintMix = cl01(tintMix);
  lk.tint = tint;
  _look = lk;
}

}  // namespace cine