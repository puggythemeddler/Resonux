#pragma once
#include "cinema/SceneFrame.h"
#include "cinema/SpatialBlock.h"
#include <stdint.h>

// TestInjector — synthetic SceneFrame source for Cinematic Mode QA (spec:
// test/demo mode + web test panel). The firmware normally consumes
// SceneFrames shipped by the companion over UDP; this injector produces the
// identical structs locally so a test/demo burst or a looping demo script can
// drive the whole engine — fusion look, room mapping waves, failsafe back to
// audio — without a companion attached.
//
// Pure C++ (no Arduino) so the host tests cover entry dwell / loop wrap /
// spatial sample / seq monotonicity. The App layer decides WHEN the injector
// runs (config `demo` flag, or a one-shot web/touch test trigger).
//
// hostTimeMs is stuffed from the receiver's own clock (loopback), so the
// engine's link-latency estimate stays ~0 and no skew is introduced.

namespace cine {

struct TestEntry {
  sceneframe::SceneKind scene = sceneframe::SCENE_ACTION;
  sceneframe::SceneEvent event = sceneframe::SEVENT_NONE;
  uint8_t conf = 100;
  uint8_t lum = 180;
  uint8_t hue = 55;
  uint8_t sat = 160;
  uint8_t val = 180;
  uint8_t motion = 120;
  int focusX = -1;        // -1 => no spatial sample this entry
  int focusY = -1;
  uint32_t lengthMs = 0;  // dwell; a 0-dwell entry "runs" for the whole script
};

class TestInjector {
 public:
  // Start a scripted run. `loop` repeats the sequence forever (demo); a
  // non-loop run deactivates once the cumulative dwell is spent (one-shot
  // burst). A restart resets the seq so the engine sees a fresh, ordered feed.
  void start(const TestEntry* seq, int n, bool loop, uint32_t nowMs) {
    _seq = seq;
    _n = n;
    _loop = loop;
    _totalMs = 0;
    for (int i = 0; i < n; ++i) _totalMs += seq[i].lengthMs;
    _baseMs = nowMs;
    _seqCounter = 0;
    _active = true;
  }
  void stop() { _active = false; }
  bool active() const { return _active; }
  bool looping() const { return _loop; }
  uint32_t baseMs() const { return _baseMs; }

  // Build the next synthetic frame for `nowMs`. Returns false once a
  // non-looping script is spent (and deactivates itself), or when stopped.
  bool step(uint32_t nowMs, sceneframe::Frame& out,
            sceneframe::SpatialInfo& sp, bool& haveSpatial) {
    if (!_active || _n <= 0) return false;
    const uint32_t elapsed = nowMs - _baseMs;
    if (!_loop && _totalMs > 0 && elapsed >= _totalMs) {
      _active = false;
      return false;
    }

    uint32_t cur = elapsed;
    if (_loop && _totalMs > 0) cur = elapsed % _totalMs;
    const TestEntry* e = _seq;
    uint32_t acc = 0;
    for (int i = 0; i < _n; ++i) {
      if (_seq[i].lengthMs > 0 && acc + _seq[i].lengthMs > cur) {
        e = &_seq[i];
        break;
      }
      acc += _seq[i].lengthMs;
    }
    if (e == nullptr) e = &_seq[0];

    const uint32_t seq = ++_seqCounter;
    packFrame(out, seq, nowMs, e->scene, e->event, e->conf, e->lum, e->hue,
              e->sat, e->val, e->motion, 128, 0);
    if (e->focusX >= 0 && e->focusY >= 0) {
      sp.focusX = (uint8_t)(e->focusX > 255 ? 255 : e->focusX);
      sp.focusY = (uint8_t)(e->focusY > 255 ? 255 : e->focusY);
      sp.zoneCount = 0;
      haveSpatial = true;
    } else {
      haveSpatial = false;
    }
    return true;
  }

 private:
  const TestEntry* _seq = nullptr;
  int _n = 0;
  bool _loop = false;
  uint32_t _totalMs = 0;
  uint32_t _baseMs = 0;
  uint32_t _seqCounter = 0;
  bool _active = false;
};

}  // namespace cine