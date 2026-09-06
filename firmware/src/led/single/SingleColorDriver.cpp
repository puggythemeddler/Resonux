#include "led/single/SingleColorDriver.h"
#include <string.h>

static const int kPwmRes = 12;
static const int kPwmMax = (1 << kPwmRes) - 1;

int SingleColorDriver::s_nextChannel = 0;

SingleColorDriver::SingleColorDriver(const StripZone* zones, int zoneCount,
                                     float maxCurrentA, float maxVolts,
                                     float pwmFreqHz, uint8_t maxBrightness)
    : _zoneCount(zoneCount), _pwmFreqHz(pwmFreqHz), _master(maxBrightness),
      _maxCurrentA(maxCurrentA), _maxVolts(maxVolts) {
  memset(_zones, 0, sizeof(_zones));
  if (zoneCount > kMaxStripZones) zoneCount = kMaxStripZones;
  for (int i = 0; i < zoneCount; ++i) _zones[i] = zones[i];
  for (int i = 0; i < kMaxStripZones; ++i) _channels[i] = -1;
}

bool SingleColorDriver::begin() {
  if (_zoneCount <= 0) return false;
  for (int i = 0; i < _zoneCount; ++i) {
    int pin = _zones[i].pins[0];
    if (pin < 0) continue;
    int ch = s_nextChannel++;
    ledcSetup(ch, _pwmFreqHz, kPwmRes);
    ledcAttachPin(pin, ch);
    _channels[i] = ch;
  }
  clear();
  return true;
}

void SingleColorDriver::clear() {
  for (int i = 0; i < _zoneCount; ++i)
    if (_channels[i] >= 0) ledcWrite(_channels[i], 0);
}

void SingleColorDriver::setPixel(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < 0 || index >= _zoneCount) return;
  if (_channels[index] < 0) return;
  float l = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
  int duty = (int)(l * _master / 255.0f * kPwmMax);
  if (duty < 0) duty = 0;
  if (duty > kPwmMax) duty = kPwmMax;
  ledcWrite(_channels[index], duty);
}