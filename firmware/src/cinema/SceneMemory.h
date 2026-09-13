#pragma once
#include "cinema/SceneFrame.h"
#include <stdint.h>

// Cinematic Scene Memory (spec §1). A bounded rolling ring of recent scene
// votes plus per-kind dwell timers and a smoothed mood-energy signal, so the
// lighting understands *what happened immediately before* rather than reacting
// to every frame in isolation.
//
// Pure C++ / Arduino-free (host-tested). Fixed-size arrays only — no heap, no
// dynamic allocation, ~250 bytes — safe to own on the LED task stack.
//
// The layer deliberately does NOT classify scenes (that is the companion's
// job) or persist anything (volatile, resets with the controller). It answers
// one question cheaply: "what is happening now, given the last few seconds?"

namespace cine {

constexpr int kSceneMemoryDepth = 16;    // ring slots (bounded history)
constexpr int kSceneEventWindowMs = 3000;// rolling "recent events" window
constexpr uint8_t kSceneRecentCap = 8;   // recentEventCount saturation
constexpr uint32_t kSceneHysteresisVotes = 3;
constexpr uint8_t kSceneChangeConfidence = 40;  // CHANGE events bypass hysteresis

// One coalesced ring entry. `kind` is the scene the event arrived in, so the
// Director can reason about transitions (whisper in QUIET vs whisper in ACTION).
struct SceneVote {
  sceneframe::SceneKind kind;
  sceneframe::SceneEvent event;
  uint8_t confidence;
  uint8_t luminance;
  uint32_t timeMs;
};

struct SceneContext {
  sceneframe::SceneKind current = sceneframe::SCENE_UNDEFINED;
  sceneframe::SceneKind previous = sceneframe::SCENE_UNDEFINED;
  uint32_t dwellMs = 0;               // time in the current scene kind
  uint32_t sinceTransitionMs = 0;     // time since the last kind switch
  float moodEnergy = 0.0f;            // smoothed energy 0..1
  bool risingEnergy = false;          // building toward a peak right now
  uint8_t recentEventCount = 0;       // events in the last kSceneEventWindowMs
};

class SceneMemory {
 public:
  // Feed one effective scene vote per LED frame. Pass SCENE_UNDEFINED when the
  // companion is absent/stale so a dead feed ages back to "unknown".
  void feed(sceneframe::SceneKind kind, sceneframe::SceneEvent ev,
            uint8_t confidence, uint8_t luminance, uint32_t nowMs) {
    _now = nowMs;
    // hysteresis: switch the current kind only after repeated agreement ...
    if (ev == sceneframe::SEVENT_CHANGE && confidence >= kSceneChangeConfidence) {
      switchCurrent(kind, nowMs);
      _candClear();
    } else if (kind != _current && kind != sceneframe::SCENE_UNDEFINED) {
      if (_candKind == kind) ++_candCount; else { _candKind = kind; _candCount = 1;
      }
      if (_candCount >= (int)kSceneHysteresisVotes) {
        switchCurrent(kind, nowMs);
        _candClear();
      }
    } else {
      _candClear();
      if (kind != sceneframe::SCENE_UNDEFINED) _dwell += _lastDt;
    }

    // coalesced ring push on discrete events (the first event after a reset
    // always lands, so a boot-time boom is not swallowed by the gap filter)
    if (ev != sceneframe::SEVENT_NONE &&
        (_count == 0 || (nowMs - _lastPushMs) >= 250u)) {
      SceneVote v;
      v.kind = kind;
      v.event = ev;
      v.confidence = confidence > 100 ? 100 : confidence;
      v.luminance = luminance;
      v.timeMs = nowMs;
      _ring[_ringHead] = v;
      _ringHead = (_ringHead + 1) % kSceneMemoryDepth;
      _count = _count < kSceneMemoryDepth ? _count + 1 : kSceneMemoryDepth;
      _lastPushMs = nowMs;
    }

    updateEnergy(ev, confidence, luminance);
    _lastDt = 16;
    if (nowMs > _lastTickMs) {
      uint32_t d = nowMs - _lastTickMs;
      _lastDt = d < 2000 ? d : 16;
    }
    _lastTickMs = nowMs;
  }

  // Fused context snapshot for the Director / status.
  SceneContext context() const {
    SceneContext c;
    c.current = _current;
    c.previous = _previous;
    c.dwellMs = _dwell;
    c.sinceTransitionMs = _lastTickMs > _switchMs ? _lastTickMs - _switchMs : 0;
    c.moodEnergy = _energy;
    c.risingEnergy = _rising;
    c.recentEventCount = recentEvents();
    return c;
  }

  void reset() {
    _current = sceneframe::SCENE_UNDEFINED;
    _previous = sceneframe::SCENE_UNDEFINED;
    _dwell = 0;
    _switchMs = 0;
    _energy = 0.0f;
    _rising = false;
    _ringHead = 0;
    _count = 0;
    _lastPushMs = 0;
    _lastTickMs = 0;
    _lastDt = 16;
    _candKind = sceneframe::SCENE_UNDEFINED;
    _candCount = 0;
  }

  // ring accessors (bounded; index 0 = oldest)
  int count() const { return _count; }
  const SceneVote& entry(int i) const { return _ring[(_ringHead - _count + i + kSceneMemoryDepth) % kSceneMemoryDepth]; }

 private:
  void _candClear() { _candKind = sceneframe::SCENE_UNDEFINED; _candCount = 0; }

  void switchCurrent(sceneframe::SceneKind kind, uint32_t nowMs) {
    if (_current == kind) return;
    _previous = _current;
    _current = kind;
    _dwell = 0;
    _switchMs = nowMs;
  }

  void updateEnergy(sceneframe::SceneEvent ev, uint8_t confidence, uint8_t luminance) {
    // target: ambient luminance floor + event push
    float evPush = 0.0f;
    switch (ev) {
      case sceneframe::SEVENT_BOOM: evPush = 0.35f; break;
      case sceneframe::SEVENT_FLASH: evPush = 0.25f; break;
      case sceneframe::SEVENT_CHANGE: evPush = 0.20f; break;
      case sceneframe::SEVENT_WHISPER: evPush = 0.12f; break;
      case sceneframe::SEVENT_DARK: evPush = 0.05f; break;
      default: break;
    }
    if (ev != sceneframe::SEVENT_NONE && confidence < 30) evPush *= 0.5f;
    float target = 0.10f + ((float)luminance / 255.0f) * 0.20f + evPush;
    if (target > 1.0f) target = 1.0f;

    // fast attack on rise, slow release on fall (dt-normalised ~16ms frames; the
    // factors are capped at 1.0 so a large first-frame gap never overshoots)
    const float dtK = (float)_lastDt / 16.0f;
    const float attack = 0.25f * dtK > 1.0f ? 1.0f : 0.25f * dtK;
    const float release = 0.008f * dtK > 1.0f ? 1.0f : 0.008f * dtK;
    const float prev = _energy;
    if (target > _energy) {
      _energy += (target - _energy) * attack;
    } else {
      _energy += (target - _energy) * release;
    }
    if (_energy < 0.0f) _energy = 0.0f;
    if (_energy > 1.0f) _energy = 1.0f;
    _rising = (recentEvents() >= 1) && (_energy > prev + 0.001f);
  }

  uint8_t recentEvents() const {
    uint8_t n = 0;
    for (int i = 0; i < _count; ++i) {
      uint32_t t = entry(i).timeMs;
      if (_now >= t && _now - t < (uint32_t)kSceneEventWindowMs) ++n;
      if (n >= kSceneRecentCap) return kSceneRecentCap;
    }
    return n;
  }

  sceneframe::SceneKind _current = sceneframe::SCENE_UNDEFINED;
  sceneframe::SceneKind _previous = sceneframe::SCENE_UNDEFINED;
  uint32_t _dwell = 0;
  uint32_t _switchMs = 0;
  uint32_t _lastTickMs = 0;
  uint32_t _lastDt = 16;
  uint32_t _now = 0;

  float _energy = 0.0f;
  bool _rising = false;

  // ring
  SceneVote _ring[kSceneMemoryDepth];
  int _ringHead = 0;
  int _count = 0;
  uint32_t _lastPushMs = 0;

  // hysteresis candidate
  sceneframe::SceneKind _candKind = sceneframe::SCENE_UNDEFINED;
  int _candCount = 0;
};

}  // namespace cine