#pragma once
#include "led/LEDDriver.h"
#include "led/LEDTypes.h"
#include <FastLED.h>

class AddressableLEDDriver : public LEDDriver {
public:
  AddressableLEDDriver(int dataPin, int clockPin, int ledCount, int chipset,
                       int colorOrder, bool reverse, uint8_t maxBrightness,
                       float maxCurrentA, float maxVolts);
  ~AddressableLEDDriver();

  bool begin() override;
  void clear() override;
  void setPixel(int index, uint8_t r, uint8_t g, uint8_t b) override;
  void show() override;
  int segmentCount() const override { return _ledCount; }
  bool isAddressable() const override { return true; }
  uint8_t channelCount() const override {
    return (_chipset == CHIP_SK6812_RGBW) ? 4 : 3;
  }
  void setMasterBrightness(uint8_t b) override {
    _master = b;
  }
  void setPowerLimit(float maxA, float volts) override {
    _maxMa = (int)(maxA * 1000.0f);
    _volts = volts;
    applyPowerLimit();
  }
  const char* driverName() const override { return "addressable"; }

private:
  void applyPowerLimit();
  static EOrder fastledOrder(int order);
  template <template<uint8_t DATA_PIN, EOrder RGB_ORDER> class Chip, EOrder Ord>
  bool addOneWire(int pin);
  template <EOrder Ord>
  bool addClocked(int pin, int clockPin);

  int      _dataPin;
  int      _clockPin;
  int      _ledCount;
  int      _chipset;
  int      _colorOrder;
  bool     _reverse;
  uint8_t  _master = 255;
  int      _maxMa = -1;
  float    _volts = 5.0f;
  CRGB*    _leds = nullptr;
};