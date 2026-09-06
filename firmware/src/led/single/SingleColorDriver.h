#pragma once
#include "led/LEDDriver.h"
#include "led/LEDTypes.h"
#include <Arduino.h>

class SingleColorDriver : public LEDDriver {
public:
  SingleColorDriver(const StripZone* zones, int zoneCount, float maxCurrentA,
                    float maxVolts, float pwmFreqHz, uint8_t maxBrightness);

  bool begin() override;
  void clear() override;
  void setPixel(int index, uint8_t r, uint8_t g, uint8_t b) override;
  void show() override {}
  int segmentCount() const override { return _zoneCount; }
  bool isAddressable() const override { return false; }
  uint8_t channelCount() const override { return 1; }
  void setMasterBrightness(uint8_t b) override { _master = b; }
  void setPowerLimit(float maxA, float volts) override {
    _maxCurrentA = maxA;
    _maxVolts = volts;
  }
  const char* driverName() const override { return "single_color"; }

private:
  StripZone _zones[kMaxStripZones];
  int       _zoneCount;
  float     _pwmFreqHz;
  uint8_t   _master = 255;
  float     _maxCurrentA = 0.0f;
  float     _maxVolts = 0.0f;
  int       _channels[kMaxStripZones] = {};
  static int s_nextChannel;
};