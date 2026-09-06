#include "led/analog/AnalogRGBDriver.h"
#include <string.h>

static const int kPwmRes = 12;
static const int kPwmMax = (1 << kPwmRes) - 1;

int AnalogRGBDriver::s_nextChannel = 0;

AnalogRGBDriver::AnalogRGBDriver(const StripZone* zones, int zoneCount,
                                 bool commonAnode, float maxCurrentA,
                                 float maxVolts, float pwmFreqHz,
                                 uint8_t maxBrightness)
    : _zoneCount(zoneCount), _commonAnode(commonAnode),
      _pwmFreqHz(pwmFreqHz), _master(maxBrightness),
      _maxCurrentA(maxCurrentA), _maxVolts(maxVolts) {
  memset(_zones, 0, sizeof(_zones));
  if (zoneCount > kMaxStripZones) zoneCount = kMaxStripZones;
  for (int i = 0; i < zoneCount; ++i) _zones[i] = zones[i];
  for (int i = 0; i < kMaxStripZones; ++i)
    for (int j = 0; j < 3; ++j) _channels[i][j] = -1;
}

bool AnalogRGBDriver::begin() {
  if (_zoneCount <= 0) return false;
  for (int i = 0; i < _zoneCount; ++i) {
    for (int j = 0; j < 3; ++j) {
      int pin = _zones[i].pins[j];
      if (pin < 0) continue;
      int ch = s_nextChannel++;
      ledcSetup(ch, _pwmFreqHz, kPwmRes);
      ledcAttachPin(pin, ch);
      _channels[i][j] = ch;
    }
  }
  clear();
  return true;
}

void AnalogRGBDriver::clear() {
  for (int i = 0; i < _zoneCount; ++i)
    for (int j = 0; j < 3; ++j)
      if (_channels[i][j] >= 0) ledcWrite(_channels[i][j], 0);
}

void AnalogRGBDriver::setPixel(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < 0 || index >= _zoneCount) return;
  uint8_t chv[3] = {r, g, b};
  for (int j = 0; j < 3; ++j) {
    if (_channels[index][j] < 0) continue;
    int duty = (int)((float)chv[j] * _master / 255.0f * kPwmMax);
    if (duty < 0) duty = 0;
    if (duty > kPwmMax) duty = kPwmMax;
    if (_commonAnode) duty = kPwmMax - duty;
    ledcWrite(_channels[index][j], duty);
  }
}