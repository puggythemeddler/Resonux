#pragma once
#include "audio/AudioFrame.h"
#include <stdint.h>
#include <math.h>

// On-device cinematic audio features (spec §9-§12, spec §23b). Pure C++, no
// Arduino: built for the host tests and cheap enough to run every audio
// frame. The companion's program-audio track provides the authoritative
// video events; this analyzer supplies the local mic fallback and the
// audio cross-check for fusion.

namespace cine {

struct AudioFeatures {
  float level = 0.0f;         // 0..1 short-term loudness (fast attack, slow release)
  float bass = 0.0f;          // 0..1
  float mid = 0.0f;           // 0..1
  float treble = 0.0f;        // 0..1
  bool  silence = false;      // near-silence sustained long enough
  bool  whisper = false;      // very quiet but audible presence
  bool  dialogue = false;     // mid-band speech-like activity
  bool  sustainedLoud = false;// window avg is loud (music etc.) — gates boom
  bool  beat = false;
  float beatStrength = 0.0f;
  float boom = 0.0f;          // 0..1 sudden bass impact (decaying envelope)
  float impact = 0.0f;        // 0..1 broadband sudden onset (decaying)
  float tension = 0.0f;       // 0..1 creeping low-dynamics mid/high (horror)
};

class SceneAnalyzer {
 public:
  // Feed one AudioFrame per call. The frame's timeMs drives the event timers
  // (dt = f.timeMs - last). Use the analyzer once per interest — the LED task
  // reuses the latest() result until the audio task publishes a new frame.
  AudioFeatures process(const AudioFrame& f);

  const AudioFeatures& latest() const { return _out; }

 private:
  AudioFeatures _out;
  float _levelFast = 0.0f;    // attack fast, release slow
  float _levelSlow = 0.0f;    // much slower window for sustained-loud/whisper
  bool  _first = true;
  uint32_t _lastMs = 0;
  // time-in-state counters (in ms since feature changes)
  uint32_t _silenceMs = 0;
  uint32_t _loudMs = 0;
  uint32_t _whisperMs = 0;
  uint32_t _speechMs = 0;
  // transient envelopes
  uint32_t _boomCdMs = 0;      // countdown before the next boom may trigger
  uint32_t _impactCdMs = 0;    // countdown before the next impact may trigger
  float _boomEnv = 0.0f;
  float _impactEnv = 0.0f;
  // short-history deltas for "sudden rise" detection
  float _baselineBass = 0.0f;   // slow-moving bass (rise is measured against it)
  float _baselineAmp = 0.0f;    // slow-moving overall level
  float _tension = 0.0f;
};

inline AudioFeatures SceneAnalyzer::process(const AudioFrame& f) {
  uint32_t dt = 16;
  if (!_first && f.timeMs >= _lastMs && f.timeMs - _lastMs < 2000) {
    dt = f.timeMs - _lastMs;
  } else if (!_first && f.timeMs < _lastMs) {
    dt = 16;  // clock wrap — treat as nominal
  }
  _lastMs = f.timeMs;
  _first = false;

  const float rawAmp =
      f.amplitude < 0.0f ? 0.0f : (f.amplitude > 1.0f ? 1.0f : f.amplitude);
  const float rawBass =
      f.bass < 0.0f ? 0.0f : (f.bass > 1.0f ? 1.0f : f.bass);
  const float rawMid =
      f.mid < 0.0f ? 0.0f : (f.mid > 1.0f ? 1.0f : f.mid);
  const float rawTreble =
      f.treble < 0.0f ? 0.0f : (f.treble > 1.0f ? 1.0f : f.treble);

  // level: fast attack, slow release
  if (rawAmp > _levelFast) {
    _levelFast += (rawAmp - _levelFast) * 0.9f;           // ~instant-ish
  } else {
    _levelFast += (rawAmp - _levelFast) * 0.06f * (dt / 16.0f);
  }
  _levelSlow += (rawAmp - _levelSlow) * 0.012f * (dt / 16.0f);
  if (_levelFast < 0.0f) _levelFast = 0.0f;
  if (_levelSlow < 0.0f) _levelSlow = 0.0f;

  const float kSilentLevel = 0.03f;
  const float kWhisperLevel = 0.12f;
  const float kLoudLevel = 0.55f;

  if (_levelFast < kSilentLevel) _silenceMs += dt; else _silenceMs = 0;
  if (_levelFast > kWhisperLevel) _whisperMs = 0;
  else if (_levelFast >= kSilentLevel) _whisperMs += dt;

  if (_levelSlow > kLoudLevel) _loudMs += dt; else _loudMs = 0;

  // dialogue: mid-band presence with contained level and low dynamics. The
  // speech-band is roughly rawMid; a speech signal swings around a moderate
  // level without hitting sustained-loud.
  const bool midActive = rawMid > 0.18f && rawMid > rawBass + 0.05f;
  const bool levelBand = _levelFast > 0.06f && _levelFast < 0.55f;
  if (levelBand && midActive) _speechMs += dt; else _speechMs = 0;

  _out.silence = _silenceMs >= 350;
  _out.whisper = _whisperMs >= 200 && _levelFast >= kSilentLevel;
  _out.sustainedLoud = _loudMs >= 1200;
  _out.dialogue = _speechMs >= 350 && !_out.sustainedLoud;

  // booms: a sudden bass rise against a slow bass baseline, only when the
  // overall program is not already sustained-loud. Cooldown + decay envelope.
  _baselineBass += (rawBass - _baselineBass) * 0.05f * (dt / 16.0f);
  _baselineAmp += (rawAmp - _baselineAmp) * 0.05f * (dt / 16.0f);
  if (_baselineBass < 0.0f) _baselineBass = 0.0f;

  const float bassRise = rawBass - _baselineBass;
  const bool boomHit =
      !_out.sustainedLoud && bassRise > 0.30f && _boomCdMs == 0;
  if (boomHit) {
    _boomCdMs = 450;
    _boomEnv = rawBass > 1.0f ? 1.0f : rawBass;
  }

  const float ampRise = rawAmp - _baselineAmp;
  const bool impactHit = ampRise > 0.40f && _impactCdMs == 0;
  if (impactHit) {
    _impactCdMs = 120;
    _impactEnv = rawAmp > 1.0f ? 1.0f : rawAmp;
  }

  if (_boomCdMs > 0) _boomCdMs = _boomCdMs > dt ? _boomCdMs - dt : 0;
  if (_impactCdMs > 0) _impactCdMs = _impactCdMs > dt ? _impactCdMs - dt : 0;
  _boomEnv *= 0.85f;   // fast decay
  _impactEnv *= 0.88f;
  if (_boomEnv < 0.01f) _boomEnv = 0.0f;
  if (_impactEnv < 0.01f) _impactEnv = 0.0f;

  // tension: low dynamics creeping mid/high = horror stingers / slow builds.
  // Rises when mid presence is high but overall level is moderate-low, decays
  // slowly, collapses when it gets loud.
  if (rawMid > 0.22f && rawTreble > 0.10f && _levelFast < 0.45f) {
    _tension += 0.0009f * dt;
  } else if (_levelFast < 0.10f && rawMid < 0.05f) {
    _tension -= 0.0006f * dt;  // genuine silence bleeds tension off
  } else {
    _tension *= (1.0f - 0.02f * (dt / 16.0f));
  }
  if (_levelFast > 0.5f) _tension *= 0.5f;  // loud moments break the tension
  if (_tension < 0.0f) _tension = 0.0f;
  if (_tension > 1.0f) _tension = 1.0f;

  float level = _levelFast;
  if (level < 0.0f) level = 0.0f;
  if (level > 1.0f) level = 1.0f;

  _out.level = level;
  _out.bass = rawBass;
  _out.mid = rawMid;
  _out.treble = rawTreble;
  _out.beat = f.beat;
  _out.beatStrength = f.beatStrength < 0.0f ? 0.0f : f.beatStrength;
  _out.boom = _boomEnv;
  _out.impact = _impactEnv;
  _out.tension = _tension;
  return _out;
}

}  // namespace cine