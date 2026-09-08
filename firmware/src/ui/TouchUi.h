#pragma once
#include "audio/AudioFrame.h"
#include "display/DisplayManager.h"
#include "display/DisplayTypes.h"

// LVGL touchscreen UI. Compiled to a no-op unless ENABLE_TOUCHUI is defined,
// so the default firmware build never links LVGL. All widgets/drivers live in
// a pimpl; hardware (panel + touch) is created from DisplayConfig.
//
// Screen set: "Now Playing" (audio bars + active theme), "Themes" (global
// theme picker), "System" (master brightness). Web + touchscreen share the
// same controller state via App.
class TouchUi {
public:
  static TouchUi& instance();

  // Creates/registers display+touch drivers from mgr.cfg() (if enabled) and
  // builds the LVGL screens. Safe to call with no panel configured.
  void begin(DisplayManager& mgr);

  void tick(uint32_t nowMs);                 // lv_tick_inc
  void render();                             // lv_timer_handler (via manager cb)
  void handleTouch(const TouchPoint& tp);    // logical -> physical -> LVGL
  void updateAudio(const AudioFrame& f);

  static void renderStatic(void*, DisplayManager&) { instance().render(); }

  bool active() const { return _impl != nullptr; }

private:
  TouchUi() {}
  TouchUi(const TouchUi&) = delete;
  struct Impl;
  Impl* _impl = nullptr;
};