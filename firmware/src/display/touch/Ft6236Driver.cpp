#include "display/touch/Ft6236Driver.h"
#include <Arduino.h>

#ifdef ENABLE_TOUCHUI

static const uint8_t kRegTdStatus = 0x02;   // touch count
static const uint8_t kRegXh1     = 0x03;   // point1 X high
static const uint8_t kRegYh1     = 0x05;   // point1 Y high

uint8_t Ft6236Driver::readReg(uint8_t reg, bool& ok) {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  if (_wire->endTransmission(false) != 0) { ok = false; return 0; }
  _wire->requestFrom(_addr, (uint8_t)1);
  if (_wire->available() < 1) { ok = false; return 0; }
  ok = true;
  return _wire->read();
}

uint16_t Ft6236Driver::readPair(uint8_t hiReg, bool& ok) {
  uint16_t v = (uint16_t)readReg(hiReg, ok) << 8;
  if (!ok) return 0;
  v |= readReg(hiReg + 1, ok) & 0x0F;
  return v;
}

bool Ft6236Driver::init() {
  _wire = &Wire;
  if (!_wire->begin((int)_cfg.touchSda, (int)_cfg.touchScl, 400000u)) {
    return false;
  }
  if (_cfg.touchRst >= 0) {
    pinMode(_cfg.touchRst, OUTPUT);
    digitalWrite(_cfg.touchRst, LOW);
    delay(5);
    digitalWrite(_cfg.touchRst, HIGH);
    delay(60);
  }
  bool ok = false;
  readReg(kRegTdStatus, ok);
  _ready = ok;
  return _ready;
}

bool Ft6236Driver::readTouch(TouchPoint& out, bool pressedRaw) {
  out = TouchPoint();
  if (!_ready) return false;
  bool ok = false;
  uint8_t n = readReg(kRegTdStatus, ok);
  if (!ok) return false;
  n &= 0x0F;
  if (n > 0 && n <= 2) {
    uint16_t x = readPair(kRegXh1, ok);
    if (!ok) return false;
    uint16_t y = readPair(kRegYh1, ok);
    if (!ok) return false;
    out.x = (int)x;
    out.y = (int)y;
    out.touched = true;
    return true;
  }
  if (n == 0) {
    out.x = -1;
    out.y = -1;
    out.touched = false;
    return true;
  }
  return false;
}

#endif  // ENABLE_TOUCHUI