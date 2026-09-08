#pragma once
#include <stdint.h>

// Graceful system control: a tiny, deliberately non-OS state machine so host
// unit-tests can exercise every transition. All real work (task spawning,
// ESP.restart/esp_deep_sleep) lives in App; this class only decides what is
// allowed and what is currently happening.
//
//   RUNNING --request--> *_REQUESTED --beginShutdown()--> SHUTTING_DOWN
//         SHUTTING_DOWN --complete()--> RESTARTING | SLEEPING
//
// A second request while already shutting down is rejected (idempotent), so
// web + touchscreen + config tab cannot stack shutdown tasks.

namespace sys {

enum class State : uint8_t {
  Running = 0,
  RestartRequested,
  PowerOffRequested,
  ShuttingDown,
  Restarting,
  Sleeping,
};

enum class Action : uint8_t {
  None = 0,
  Restart,
  PowerOff,
};

class SystemMode {
public:
  // Queues a command. True only on a Running -> *_Requested transition.
  bool request(Action a) {
    if (a == Action::None) return false;
    if (_state != State::Running) return false;
    _action = a;
    _state = (a == Action::Restart) ? State::RestartRequested
                                    : State::PowerOffRequested;
    return true;
  }

  // The shutdown task takes over: command has been consumed past the point of
  // no return (outputs must now be quiesced).
  void beginShutdown() { _state = State::ShuttingDown; }

  // Called just before the terminal action; resolves to the final state so
  // callers can distinguish restart from sleep.
  void complete() {
    _state = (_action == Action::Restart) ? State::Restarting : State::Sleeping;
  }

  State state() const { return _state; }
  Action pending() const { return _action; }
  bool shuttingDown() const { return _state != State::Running; }

  const char* stateName() const {
    switch (_state) {
      case State::Running:           return "running";
      case State::RestartRequested:  return "restart_requested";
      case State::PowerOffRequested: return "power_off_requested";
      case State::ShuttingDown:      return "shutting_down";
      case State::Restarting:        return "restarting";
      case State::Sleeping:          return "sleeping";
    }
    return "unknown";
  }

  const char* actionName() const {
    switch (_action) {
      case Action::Restart:  return "restart";
      case Action::PowerOff: return "power_off";
      case Action::None:     return "none";
    }
    return "unknown";
  }

private:
  State  _state = State::Running;
  Action _action = Action::None;
};

}  // namespace sys