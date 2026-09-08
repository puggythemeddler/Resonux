#pragma once
#include "display/DisplayTypes.h"
#include "display/TouchDriver.h"

#ifdef ENABLE_TOUCHUI

#include <Wire.h>

// FT6236 capacitive touch (most 3.5" 320x480 SPI modules). I2C, returns the
// first tracked point in raw PANEL coordinates.

class Ft6236Driver : public TouchDriver {
public:
  explicit Ft6236Driver(const DisplayConfig& cfg) : _cfg(cfg) {}

  bool init() override;
  const char* name() const override { return "FT6236"; }
  bool readTouch(TouchPoint& out, bool pressedRaw) override;

private:
  uint8_t readReg(uint8_t reg, bool& ok);
  uint16_t readPair(uint8_t hiReg, bool& ok);

  const DisplayConfig& _cfg;
  TwoWire* _wire = nullptr;
  uint8_t _addr = 0x38;
  bool _ready = false;
};

#endif  // ENABLE_TOUCHUI