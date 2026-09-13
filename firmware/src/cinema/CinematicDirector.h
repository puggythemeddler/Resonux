#pragma once
#include "audio/AudioFrame.h"
#include "cinema/CinematicConfig.h"
#include "cinema/SceneAnalyzer.h"
#include "cinema/SceneFrame.h"
#include "cinema/SceneMemory.h"
#include <stdint.h>
#include <math.h>

// Cinematic Director / Intent layer (spec §17). A lightweight deterministic
// "what should happen next?" controller on top of Scene Memory + audio + video.
//
// It produces a CinematicIntent — mood, sustained energy target, tint warmth,
// pulse depth, flash suppression and a minimum hold time — that the
// CinematicEngine folds into its existing envelopes. The Director never touches
// Look fields or hardware directly; all safety constraints (flash gap,
// brightness ceiling, boom cooldown) stay in the engine.
//
// Pure C++ / Arduino-free, tiny (a handful of scalars), host-tested. No AI: the
// mood cascade below is a fixed deterministic rule table over bounded inputs.

namespace cine {

enum Mood : uint8_t {
  MOOD_CALM = 0,
  MOOD_SUSPENSE,
  MOOD_TENSION,
  MOOD_ACTION,
  MOOD_IMPACT,
  MOOD_AFTERMATH,
  MOOD_TRANSITION,
  MOOD_PERFORMANCE,
  MOOD_COUNT,
};

struct CinematicIntent {
  Mood mood = MOOD_CALM;
  float energy = 0.0f;        // 0..1 sustained energy target
  float tintWarmth = 0.5f;    // 0..1 cool -> warm
  float pulseDepth = 1.0f;    // 0..1 beat-pulse multiplier
  float flashScale = 1.0f;    // 0..1 flash aggression (<=1, never exceeds ceiling)
  uint32_t holdMs = 250;      // min duration before the mood may change
};

inline const char* moodIdent(Mood m) {
  switch (m) {
    case MOOD_SUSPENSE: return "suspense";
    case MOOD_TENSION: return "tension";
    case MOOD_ACTION: return "action";
    case MOOD_IMPACT: return "impact";
    case MOOD_AFTERMATH: return "aftermath";
    case MOOD_TRANSITION: return "transition";
    case MOOD_PERFORMANCE: return "performance";
    default: return "calm";
  }
}

inline const char* moodLabel(Mood m) {
  switch (m) {
    case MOOD_SUSPENSE: return "Suspense";
    case MOOD_TENSION: return "Tension";
    case MOOD_ACTION: return "Action";
    case MOOD_IMPACT: return "Impact";
    case MOOD_AFTERMATH: return "Aftermath";
    case MOOD_TRANSITION: return "Transition";
    case MOOD_PERFORMANCE: return "Performance";
    default: return "Calm";
  }
}

class CinematicDirector {
 public:
  // ctx = SceneMemory::context(); audio = SceneAnalyzer features; scene =
  // freshest accepted SceneFrame (nullptr when the feed is stale/absent);
  // cfg = cinematic config (mode, genre, reaction, influences).
  CinematicIntent compute(const SceneContext& ctx, const AudioFeatures& audio,
                          const sceneframe::Frame* scene, const Config& cfg,
                          uint32_t nowMs);

  void reset() {
    _mood = MOOD_CALM;
    _holdUntilMs = 0;
    _aftermathUntilMs = 0;
  }

 private:
  Mood _candidate(const SceneContext& ctx, const AudioFeatures& audio,
                  const sceneframe::Frame* scene, const Config& cfg,
                  uint32_t nowMs);
  void _applyGenre(CinematicIntent& in, const SceneContext& ctx,
                   const AudioFeatures& audio, const Config& cfg);

  Mood _mood = MOOD_CALM;
  uint32_t _holdUntilMs = 0;
  uint32_t _aftermathUntilMs = 0;
};

inline Mood CinematicDirector::_candidate(const SceneContext& ctx,
                                          const AudioFeatures& audio,
                                          const sceneframe::Frame* scene,
                                          const Config& cfg, uint32_t nowMs) {
  (void)cfg;
  const bool video = scene != nullptr;
  const sceneframe::SceneEvent ev = video ? (sceneframe::SceneEvent)scene->eventId
                                          : sceneframe::SEVENT_NONE;
  const uint8_t conf = video ? scene->eventConfidence : 0;

  // 1. discrete impact: boom or flash with real confidence
  if (video && conf >= 50) {
    if (ev == sceneframe::SEVENT_BOOM) { _aftermathUntilMs = nowMs + 600u; return MOOD_IMPACT; }
    if (ev == sceneframe::SEVENT_FLASH && conf >= 55) { _aftermathUntilMs = nowMs + 500u; return MOOD_IMPACT; }
  }

  // 2. aftermath: right after an impact, decay toward calm
  if (_aftermathUntilMs != 0 && nowMs < _aftermathUntilMs) return MOOD_AFTERMATH;
  _aftermathUntilMs = 0;

  // 3. hard scene cut
  if (video && ev == sceneframe::SEVENT_CHANGE) return MOOD_TRANSITION;

  // 4. long-static scene reads as a transition cue
  if (ctx.dwellMs > 8000) return MOOD_TRANSITION;

  // 5. music / performance (explicit scene, or strong beat energy)
  if ((video && (sceneframe::SceneKind)scene->sceneId == sceneframe::SCENE_MUSIC) ||
      (!video && audio.beat && audio.beatStrength > 0.5f && ctx.moodEnergy > 0.4f)) {
    return MOOD_PERFORMANCE;
  }

  // 6. energy cascade
  const float e = ctx.moodEnergy;
  if (e < 0.18f) return MOOD_CALM;

  bool tensionPush = audio.tension > 0.4f;
  if (e < 0.45f) {
    return (ctx.risingEnergy || tensionPush) ? MOOD_SUSPENSE : MOOD_CALM;
  }
  if (e < 0.70f) {
    return ctx.risingEnergy ? MOOD_ACTION : (tensionPush ? MOOD_TENSION : MOOD_SUSPENSE);
  }
  return MOOD_ACTION;
}

inline void CinematicDirector::_applyGenre(CinematicIntent& in,
                                           const SceneContext& ctx,
                                           const AudioFeatures& audio,
                                           const Config& cfg) {
  const int g = cfg.genre;
  switch (g) {
    case GENRE_HORROR:
      // tension lingers; flashes stay muted outside full action
      if (in.mood == MOOD_TENSION || in.mood == MOOD_SUSPENSE ||
          in.mood == MOOD_CALM) {
        in.flashScale = in.flashScale > 0.6f ? 0.6f : in.flashScale;
        in.holdMs += 150;
        if (in.mood != MOOD_CALM) in.tintWarmth = 0.25f;  // cold/cyan dread
      }
      break;
    case GENRE_ANIME:
      // snappier, brighter, more motion
      in.pulseDepth = in.pulseDepth > 1.2f ? in.pulseDepth : 1.2f;
      if (in.mood == MOOD_ACTION || in.mood == MOOD_IMPACT) {
        in.flashScale = 1.0f;
        in.tintWarmth = 0.75f;  // warm energy
      }
      break;
    case GENRE_AUTO: {
      // heuristic: derive "feel" from memory + audio, never pretend to know genre
      bool tensionish = (ctx.current == sceneframe::SCENE_QUIET ||
                         ctx.current == sceneframe::SCENE_SPEECH) &&
                        (ctx.moodEnergy < 0.55f) && audio.tension > 0.25f;
      bool actionish = (ctx.current == sceneframe::SCENE_ACTION ||
                        ctx.current == sceneframe::SCENE_CHASE) &&
                       ctx.moodEnergy >= 0.55f;
      if (tensionish && (in.mood == MOOD_SUSPENSE || in.mood == MOOD_TENSION)) {
        in.flashScale = in.flashScale > 0.7f ? 0.7f : in.flashScale;
        in.tintWarmth = 0.3f;
      } else if (actionish && in.mood == MOOD_ACTION) {
        in.pulseDepth = in.pulseDepth > 1.1f ? in.pulseDepth : 1.1f;
        in.tintWarmth = 0.7f;
      }
      break;
    }
    default:
      break;
  }
}

inline CinematicIntent CinematicDirector::compute(const SceneContext& ctx,
                                                  const AudioFeatures& audio,
                                                  const sceneframe::Frame* scene,
                                                  const Config& cfg,
                                                  uint32_t nowMs) {
  CinematicIntent in;
  in.energy = ctx.moodEnergy;

  // mood switch is gated by holdMs so sustained-level moods (CALM..ACTION) don't
  // oscillate every 250 ms. Discrete IMPACTs punch straight through the hold —
  // a real boom/flash must never be delayed or dropped while we "hold a mood".
  Mood next = _candidate(ctx, audio, scene, cfg, nowMs);
  const bool armed = (_holdUntilMs != 0);
  if (next == MOOD_IMPACT && _mood != MOOD_IMPACT) {
    _mood = MOOD_IMPACT;
    _holdUntilMs = nowMs + 400;
  } else {
    const bool canSwitch = !armed || (nowMs >= _holdUntilMs);
    if (canSwitch && (!armed || next != _mood)) {
      _mood = next;
      uint32_t hold = 250;
      if (_mood == MOOD_AFTERMATH) hold = 300;
      else if (_mood == MOOD_TRANSITION) hold = 300;
      _holdUntilMs = nowMs + hold;
    }
  }
  in.mood = _mood;
  in.holdMs = _holdUntilMs > nowMs ? (_holdUntilMs - nowMs) : 0;

  // per-mood shaping (flashScale never exceeds 1.0 — the engine ceiling holds)
  switch (_mood) {
    case MOOD_CALM: in.flashScale = 0.4f; in.pulseDepth = 0.7f;
                    in.tintWarmth = 0.5f; break;
    case MOOD_SUSPENSE: in.flashScale = 0.6f; in.pulseDepth = 0.8f;
                        in.tintWarmth = 0.4f; break;
    case MOOD_TENSION: in.flashScale = 0.85f; in.pulseDepth = 0.9f;
                       in.tintWarmth = 0.25f; break;
    case MOOD_ACTION: in.flashScale = 1.0f; in.pulseDepth = 1.15f;
                      in.tintWarmth = 0.7f; break;
    case MOOD_IMPACT: in.flashScale = 1.0f; in.pulseDepth = 1.0f;
                      in.tintWarmth = 0.75f; break;
    case MOOD_AFTERMATH: in.flashScale = 0.5f; in.pulseDepth = 0.8f;
                         in.tintWarmth = 0.85f; break;  // ember warm
    case MOOD_TRANSITION: in.flashScale = 0.75f; in.pulseDepth = 1.0f;
                          in.tintWarmth = 0.5f; break;
    case MOOD_PERFORMANCE: in.flashScale = 1.0f; in.pulseDepth = 1.2f;
                           in.tintWarmth = 0.6f; break;
    default: break;
  }

  _applyGenre(in, ctx, audio, cfg);
  return in;
}

}  // namespace cine