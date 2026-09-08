#pragma once
#include <stdint.h>

// Slave-side clock estimation: maps master millis() into the local timeline.
// Pure C++ (native-testable). LAN transit is assumed symmetric, so one packet
// gives offset ~= (localArrival - remoteSend)/2. The first observation seeds
// the estimate; later ones converge exponentially. uint32 wrap is handled by
// subtraction semantics, so very different boot upticks still converge.

class SyncClock {
 public:
  void reset() {
    _offsetMs = 0;
    _have = false;
  }

  // localNowMs: local millis() when the packet arrived.
  // remoteTimeMs: master millis() stamped in the packet (AudioFrame.timeMs).
  void observe(uint32_t localNowMs, uint32_t remoteTimeMs) {
    int32_t latency = (int32_t)(localNowMs - remoteTimeMs);  // wrap-safe
    int32_t sample = latency / 2;
    if (!_have) {
      _offsetMs = sample;
      _have = true;
    } else {
      _offsetMs += (sample - _offsetMs) / 4;  // fast convergence, jitter damped
    }
  }

  // Estimated fixed offset: local = remote + offsetMs.
  int32_t offsetMs() const { return _offsetMs; }
  bool have() const { return _have; }

  // Translate a master timestamp into the local clock (wrap-safe).
  uint32_t toLocal(uint32_t remoteTimeMs) const {
    return (uint32_t)((int64_t)remoteTimeMs + _offsetMs);
  }

 private:
  int32_t _offsetMs = 0;
  bool _have = false;
};