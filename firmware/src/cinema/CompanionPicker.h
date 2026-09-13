#pragma once
#include <stdint.h>
#include <string.h>

// CompanionPicker — sticky source selection for the cinematic companion feed
// (spec: source-identity hardening). SceneFrames arrive over multicast UDP as
// a (ip:port) stream; a second companion on the same group/port, or a stale
// one that comes back with a reseted counter, must never corrupt the picture
// the engine is currently rendering.
//
// Pure C++ (no Arduino / no FreeRTOS) so the host unit tests exercise the whole
// decision matrix. Transport lives in SceneLinkNode.
//
// Rules (deterministic):
//   * Sources are keyed by UDP src ip:port. Only frames from the ACCEPTED
//     (active) source are handed to the engine; other sources are tracked but
//     ignored while the active source is still live, so a flaky companion can
//     never cause mid-scene flicker or source ping-pong.
//   * Per-source ordering uses a rolling counter guard: a frame is accepted
//     only when (int32_t)(seq - lastSeq) > 0. Seq wraps 4B fine; duplicate,
//     late and out-of-order packets are dropped without any wire change.
//   * The active source switches ONLY when it goes stale (quiet > staleMs).
//     Among still-alive candidates it prefers: highest validCount, then most
//     recent frame, then lowest ip. Ties fall to the lowest slot index.
//   * The table holds up to kMaxSources (4). When full, a new source evicts
//     the least-deserving NON-ACTIVE slot: lowest validCount, then oldest
//     lastRx, then highest slot index. The active slot is never evicted, so a
//     working feed can't be pushed out by noisy neighbours.
//   * A clock-skew estimate (ppm) is maintained against the active source's
//     hostTimeMs so the web UI can show how far the companion clock has drifted
//     from the receiver clock.

namespace cine {

struct SourceKey {          // UDP src identity, stored in host byte order
  uint32_t ip = 0;
  uint16_t port = 0;
};

inline bool sameKey(const SourceKey& a, const SourceKey& b) {
  return a.ip == b.ip && a.port == b.port;
}

enum SourceAccept : uint8_t {
  ACCEPT_COMMIT = 0,       // fresh, ordered frame from the active source (or a
                           // switch to it) — hand the frame to the engine
  REJECT_REPLAY,           // duplicate / out-of-order / wrapped-negative seq
  REJECT_INACTIVE,         // tracked source, but the active one is still live
  REJECT_FULL,             // table full with no evictable non-active slot
};

struct SourceInfo {
  SourceKey key;
  uint32_t seq = 0;          // last accepted seq from this source
  uint32_t hostTimeMs = 0;   // last companion clock value (for skew estimate)
  uint32_t lastRxMs = 0;     // local rx time of the last accepted frame
  uint32_t validCount = 0;   // accepted frames (activity/votes metric)
  bool active = false;
};

class CompanionPicker {
 public:
  static constexpr int kMaxSources = 4;

  void configure(uint32_t staleMs) { _staleMs = staleMs; }
  void reset() {
    for (int i = 0; i < kMaxSources; ++i) _tab[i] = SourceInfo();
    _active = -1;
    _seedLocalMs = 0;
    _seedHostMs = 0;
    _skewPpm = 0;
    _haveSkew = false;
  }

  // Decide the disposition of one received frame. `nowMs` is local millis() of
  // delivery. Call once per (key, seq, hostMs) tuple, in arrival order.
  SourceAccept accept(const SourceKey& key, uint32_t seq, uint32_t hostMs,
                      uint32_t nowMs) {
    _lastNow = nowMs;
    int idx = findOrAdd(key);
    if (idx < 0) return REJECT_FULL;
    SourceInfo& e = _tab[idx];

    // ordering + replay guard (rolling counter)
    if (e.validCount > 0 && (int32_t)(seq - e.seq) <= 0) return REJECT_REPLAY;

    e.seq = seq;
    e.hostTimeMs = hostMs;
    e.lastRxMs = nowMs;
    e.validCount++;

    if (_active < 0) {
      setActive(idx, nowMs);
      return ACCEPT_COMMIT;
    }
    if (_active == idx) {
      updateSkew(e, nowMs);
      return ACCEPT_COMMIT;
    }

    // active source elsewhere: don't switch mid-scene; keep tracking.
    const bool activeLive =
        (int32_t)(nowMs - _tab[_active].lastRxMs) < (int32_t)_staleMs;
    if (activeLive) return REJECT_INACTIVE;

    // active source is stale — promote the best still-alive candidate. The
    // sender is by construction the most recent, so it wins ties on recency,
    // but a source with more votes and a fresh-enough packet wins outright.
    const int best = bestAliveCandidate();
    if (best != idx) {
      setActive(best, nowMs);
      return REJECT_INACTIVE;  // the sender's slot won, not its frame
    }
    setActive(idx, nowMs);
    return ACCEPT_COMMIT;
  }

  int indexOf(const SourceKey& key) const {
    for (int i = 0; i < kMaxSources; ++i)
      if (_tab[i].validCount > 0 && sameKey(_tab[i].key, key)) return i;
    return -1;
  }

  int sourceCount() const {
    int n = 0;
    for (int i = 0; i < kMaxSources; ++i)
      if (_tab[i].validCount > 0) ++n;
    return n;
  }

  int activeIndex() const { return _active; }
  const SourceInfo* active() const {
    return _active >= 0 ? &_tab[_active] : nullptr;
  }
  const SourceInfo* info(int i) const {
    return (i >= 0 && i < kMaxSources) ? &_tab[i] : nullptr;
  }

  uint32_t lastRxMs() const {
    return _active >= 0 ? _tab[_active].lastRxMs : 0;
  }
  int32_t skewPpm() const { return _skewPpm; }

 private:
  int findOrAdd(const SourceKey& key) {
    int i = indexOf(key);
    if (i >= 0) return i;
    for (int j = 0; j < kMaxSources; ++j) {
      if (_tab[j].validCount == 0) {
        _tab[j].key = key;
        return j;
      }
    }
    // full: evict the least-deserving non-active slot
    int victim = -1;
    for (int j = 0; j < kMaxSources; ++j) {
      if (j == _active) continue;
      if (victim < 0 || worseKeeper(j, victim)) victim = j;
    }
    if (victim < 0) return -1;
    _tab[victim] = SourceInfo();
    _tab[victim].key = key;
    return victim;
  }

  // true when entry a is a worse keeper than b (oldest-first ordering)
  bool worseKeeper(int a, int b) const {
    if (_tab[a].validCount != _tab[b].validCount)
      return _tab[a].validCount < _tab[b].validCount;
    if (_tab[a].lastRxMs != _tab[b].lastRxMs)
      return _tab[a].lastRxMs < _tab[b].lastRxMs;
    return a > b;  // deterministic: highest index goes first
  }

  // best still-alive candidate (validCount desc, lastRx desc, ip asc, slot asc)
  int bestAliveCandidate() const {
    int best = -1;
    for (int i = 0; i < kMaxSources; ++i) {
      if (_tab[i].validCount == 0) continue;
      if (_tab[i].lastRxMs == 0) continue;
      if ((int32_t)(_lastNow - _tab[i].lastRxMs) >= (int32_t)_staleMs) continue;
      if (best < 0 || outranks(i, best)) best = i;
    }
    return best;
  }

  bool outranks(int a, int b) const {
    if (_tab[a].validCount != _tab[b].validCount)
      return _tab[a].validCount > _tab[b].validCount;
    if (_tab[a].lastRxMs != _tab[b].lastRxMs)
      return _tab[a].lastRxMs > _tab[b].lastRxMs;
    if (_tab[a].key.ip != _tab[b].key.ip)
      return _tab[a].key.ip < _tab[b].key.ip;
    if (_tab[a].key.port != _tab[b].key.port)
      return _tab[a].key.port < _tab[b].key.port;
    return a < b;
  }

  void setActive(int idx, uint32_t nowMs) {
    for (int i = 0; i < kMaxSources; ++i)
      if (i != idx) _tab[i].active = false;
    _tab[idx].active = true;
    _active = idx;
    _seedLocalMs = nowMs;
    _seedHostMs = _tab[idx].hostTimeMs;
    _skewPpm = 0;
    _haveSkew = false;
  }

  void updateSkew(const SourceInfo& e, uint32_t nowMs) {
    const uint32_t dl = nowMs - _seedLocalMs;
    if (dl < 1000) return;  // wait for a stable baseline
    const int64_t dh = (int64_t)e.hostTimeMs - (int64_t)_seedHostMs;
    const int32_t cur =
        (int32_t)((dh - (int64_t)dl) * 1000000LL / (int64_t)dl);
    if (!_haveSkew) {
      _skewPpm = cur;
      _haveSkew = true;
    } else {
      _skewPpm = (int32_t)(((int64_t)_skewPpm * 3 + cur) / 4);
    }
  }

  SourceInfo _tab[kMaxSources];
  int _active = -1;
  uint32_t _staleMs = 1200;
  uint32_t _lastNow = 0;  // cached for bestAliveCandidate() recency filter
  uint32_t _seedLocalMs = 0;
  uint32_t _seedHostMs = 0;
  int32_t _skewPpm = 0;
  bool _haveSkew = false;
};

}  // namespace cine