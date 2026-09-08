#pragma once
#include "display/DisplayTypes.h"
#include <stdint.h>

// Abstract touch driver. Raw samples are returned in PANEL coordinates
// (0..panelWidth-1, 0..panelHeight-1); DisplayManager maps them to logical
// UI coordinates. Implementations: FT6236 (I2C), XPT2046 (SPI).
// No touch hardware attached => use NoTouchDriver (returns TouchPoint{}).

class TouchDriver {
public:
  virtual ~TouchDriver() {}

  virtual bool init() = 0;
  virtual const char* name() const = 0;

  // Non-blocking poll of "pressed" plus calibrated panel coordinates.
  // Returns true when the reading is valid (even if released — sets touched=false).
  virtual bool readTouch(TouchPoint& out, bool pressedRaw) = 0;

  // Optional streaming read used by the manager's poll loop.
  // Default forwards to readTouch with (false).
  virtual bool poll(TouchPoint& out);
};

class NoTouchDriver : public TouchDriver {
public:
  bool init() override { return true; }
  const char* name() const override { return "none"; }
  bool readTouch(TouchPoint&, bool) override { return true; }
};