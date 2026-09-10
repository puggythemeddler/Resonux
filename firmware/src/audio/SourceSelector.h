#pragma once
#include "audio/SourceKind.h"
#include <stdint.h>
#include <string.h>

// Automatic audio-source selection policy. Pure C++ and host-testable.
//
// Rules honoured here:
//   * Auto mode prefers the configured "preferred" source while it is
//     available, and falls back to the "fallback" source otherwise.
//   * Switches are NEVER instant or silent: a change must hold as the
//     candidate for a debounce period before it commits, and there is a hard
//     minimum dwell between committed switches (hysteresis + anti flap).
//   * With auto OFF the current source stays put, even if it goes briefly
//     unavailable — the caller reports the gap instead of surprising the user.
//   * Manual selection is instantaneous and reported as such.

namespace dev {

constexpr int kSourceMaskFrom = SOURCE_NONE;
constexpr int kSourceMaskTo   = SOURCE_COUNT;

inline uint32_t sourceMaskOf(SourceKind k) {
  return (k >= SOURCE_NONE && k < SOURCE_COUNT) ? (1u << (int)k) : 0u;
}

inline bool sourceKindAvailable(uint32_t mask, SourceKind k) {
  return (mask & sourceMaskOf(k)) != 0u;
}

enum class ChangeReason : uint8_t {
  None = 0,
  Manual,
  AutoPreferred,  // auto moved back to the preferred source
  AutoFallback,   // auto moved to the fallback source
  AutoCandidate,  // a switch is being prepared (debounce/hysteresis)
  Unavailable,    // current source is not currently available (no switch)
  Pending,        // manual choice differs from committed state
};

class SourceSelector {
public:
  SourceSelector() {
    configure(false, SOURCE_MIC, SOURCE_NONE, SOURCE_MIC);
  }

  void configure(bool autoSelect, SourceKind preferred, SourceKind fallback,
                 SourceKind manualDefault) {
    _autoSelect = autoSelect;
    _preferred = preferred;
    _fallback = (fallback >= SOURCE_NONE && fallback < SOURCE_COUNT)
                    ? fallback : SOURCE_NONE;
    _current = (manualDefault >= SOURCE_NONE && manualDefault < SOURCE_COUNT)
                   ? manualDefault : SOURCE_NONE;
    _pendingTarget = SOURCE_NONE;
    _pending = false;
    _reason = ChangeReason::None;
  }

  void setManual(SourceKind k, uint32_t nowMs) {
    if (k < SOURCE_NONE || k >= SOURCE_COUNT) return;
    _current = k;
    _pending = false;
    _reason = ChangeReason::Manual;
    _lastSwitchMs = nowMs;
  }

  SourceKind current() const { return _current; }
  ChangeReason reason() const { return _reason; }

  const char* reasonText() const {
    switch (_reason) {
      case ChangeReason::None: return "";
      case ChangeReason::Manual: return "manual";
      case ChangeReason::AutoPreferred: return "auto_preferred";
      case ChangeReason::AutoFallback: return "auto_fallback";
      case ChangeReason::AutoCandidate: return "auto_waiting";
      case ChangeReason::Unavailable: return "source_unavailable";
      case ChangeReason::Pending: return "manual_change_pending";
    }
    return "";
  }

  // Availability is an up-to-the-call bitmask. Updates the committed state and
  // returns the (possibly unchanged) active source + the reason for the last
  // change. `nowMs` is monotonically increasing.
  SourceKind update(uint32_t nowMs, uint32_t availabilityMask) {
    if (!_autoSelect) {
      if (!sourceKindAvailable(availabilityMask, _current) &&
          _current != SOURCE_NONE) {
        _reason = ChangeReason::Unavailable;
      } else if (_reason == ChangeReason::Unavailable) {
        _reason = ChangeReason::None;
      }
      return _current;
    }

    const bool prefOk = sourceKindAvailable(availabilityMask, _preferred);
    SourceKind target = SOURCE_NONE;
    if (prefOk) {
      target = _preferred;
    } else if (_preferred != SOURCE_NONE) {
      // preferred gone: fall back only if a fallback is configured & present
      if (sourceKindAvailable(availabilityMask, _fallback)) target = _fallback;
    } else if (sourceKindAvailable(availabilityMask, _fallback)) {
      target = _fallback;
    }

    if (target == _current) {
      _pending = false;
      _reason = (_reason == ChangeReason::AutoPreferred ||
                 _reason == ChangeReason::AutoFallback)
                    ? ChangeReason::None
                    : _reason;
      if (_reason == ChangeReason::AutoCandidate) _reason = ChangeReason::None;
      return _current;
    }

    // A change is warranted: arm (or re-arm) the debounce.
    if (!_pending || _pendingTarget != target) {
      _pendingTarget = target;
      _pending = true;
      _pendingSince = nowMs;
      _reason = ChangeReason::AutoCandidate;
      return _current;
    }

    if (nowMs >= _pendingSince + kHoldMs && nowMs >= _lastSwitchMs + kDwellMs) {
      _current = target;
      _pending = false;
      _reason = (target == _preferred) ? ChangeReason::AutoPreferred
                                       : ChangeReason::AutoFallback;
      _lastSwitchMs = nowMs;
    }
    return _current;
  }

  // Test hooks: expose the timing so host tests use realistic values.
  static uint32_t holdMs() { return kHoldMs; }
  static uint32_t dwellMs() { return kDwellMs; }

private:
  static const uint32_t kHoldMs  = 1500u;  // candidate debounce
  static const uint32_t kDwellMs = 2500u;  // minimum time between commits

  bool       _autoSelect = false;
  SourceKind _preferred = SOURCE_MIC;
  SourceKind _fallback = SOURCE_NONE;
  SourceKind _current = SOURCE_MIC;
  SourceKind _pendingTarget = SOURCE_NONE;
  bool       _pending = false;
  uint32_t   _pendingSince = 0;
  uint32_t   _lastSwitchMs = 0;
  ChangeReason _reason = ChangeReason::None;
};

}  // namespace dev