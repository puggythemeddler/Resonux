#pragma once
#include "config/Config.h"
#include "display/DisplayDriver.h"
#include "display/DisplayTypes.h"
#include "display/TouchDriver.h"
#include <stdint.h>

// Hardware-independent display service.
//
// * Owns an optional DisplayDriver + TouchDriver (nullptr == no hardware).
// * Exposes LOGICAL coordinates (reference 320x480, responsive across
//   320x240..800x480) and maps them to the physical panel via scaling,
//   centering and orientation — UI code never knows the panel resolution.
// * Keeps SCREEN brightness (backlight %) fully separate from LED brightness.
// * Implements the screen timeout: the screen sleeps after inactivity but
//   audio, LED rendering and the web dashboard keep running. Any touch wakes
//   it.
// * Enforces a controlled UI frame rate (default 30 fps).

class DisplayManager {
public:
  static DisplayManager& instance();

  // Drivers may be null — the manager then becomes a safe no-op.
  void begin(const DisplayConfig& cfg, DisplayDriver* display = nullptr,
             TouchDriver* touch = nullptr);

  bool enabled() const { return _enabled; }
  bool active() const { return _active; }
  const DisplayConfig& cfg() const { return _cfg; }
  DisplayDriver* display() { return _display; }
  TouchDriver* touch() { return _touch; }

  int logicalW() const { return _cfg.logicalW; }
  int logicalH() const { return _cfg.logicalH; }
  int panelW() const { return _panelW; }
  int panelH() const { return _panelH; }

  // UI render callback invoked (up to `uiFps` times/s) by update() when the
  // screen is awake. Coordinates given to the callback are already physical.
  typedef void (*DrawCallback)(void* ctx, DisplayManager& mgr);
  void setDrawCallback(DrawCallback cb, void* ctx = nullptr) {
    _drawCb = cb; _drawCtx = ctx;
  }

  // Run once per main loop tick. Handles timeout + controlled-rate render.
  void update(uint32_t nowMs);

  void wake();
  void sleep();
  bool isAwake() const { return _awake; }

  // Screen brightness (0..100). Does NOT touch LED brightness.
  void setBacklightPct(uint8_t pct);
  uint8_t backlightPct() const { return _backlightPct; }

  // Physical-bounds helpers for drawing primitives through the transform.
  void drawPixel(int lx, int ly, uint16_t c);
  void fillRect(int lx, int ly, int w, int h, uint16_t c);
  void flush();

  // Physical-coordinate bulk write (LVGL flush path).
  void flushArea(int x, int y, int w, int h, const uint16_t* pixels) {
    if (_active && _display) _display->writeArea(x, y, w, h, pixels);
  }

  // Map a logical point to physical panel pixels (same transform as fillRect).
  void mapPoint(int lx, int ly, int& px, int& py) const {
    float s = _scale;
    if (_swapAxes) {
      px = _ox + (int)(ly * s);
      py = _oy + (int)((_cfg.logicalW - 1 - lx) * s);
    } else {
      px = _ox + (int)(lx * s);
      py = _oy + (int)(ly * s);
    }
  }

  // Touch (logical coords, already orientation/scale-mapped).
  bool pollTouch(TouchPoint& out);

  uint32_t lastActivityMs() const { return _lastActivityMs; }

private:
  void recomputeTransform();
  bool mapRect(int lx, int ly, int w, int h, int& px, int& py, int& pw,
               int& ph) const;

  DisplayConfig   _cfg;
  bool            _enabled = false;
  bool            _awake = true;
  bool            _active = false;
  DisplayDriver*  _display = nullptr;
  TouchDriver*    _touch = nullptr;

  int          _panelW = 0;
  int          _panelH = 0;
  float        _scale = 1.0f;
  int          _ox = 0;
  int          _oy = 0;
  bool         _swapAxes = false;

  uint8_t      _backlightPct = 70;
  uint32_t     _lastActivityMs = 0;
  uint32_t     _lastRenderMs = 0;

  DrawCallback _drawCb = nullptr;
  void*        _drawCtx = nullptr;
};