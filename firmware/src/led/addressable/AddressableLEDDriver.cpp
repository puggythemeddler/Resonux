#include "led/addressable/AddressableLEDDriver.h"
#include <string.h>

AddressableLEDDriver::AddressableLEDDriver(int dataPin, int clockPin,
                                           int ledCount, int chipset,
                                           int colorOrder, bool reverse,
                                           uint8_t maxBrightness,
                                           float maxCurrentA, float maxVolts)
    : _dataPin(dataPin), _clockPin(clockPin), _ledCount(ledCount),
      _chipset(chipset), _colorOrder(colorOrder), _reverse(reverse),
      _master(maxBrightness) {
  if (maxCurrentA > 0.0f) {
    _maxMa = (int)(maxCurrentA * 1000.0f);
    _volts = maxVolts;
  }
}

AddressableLEDDriver::~AddressableLEDDriver() {
  delete[] _leds;
  _leds = nullptr;
}

template <typename Chip, EOrder Ord>
void AddressableLEDDriver::addOneWire(int pin) {
  FastLED.addLeds<Chip, Ord>(_leds, _ledCount, pin);
  _added = true;
}

template <typename Chip, EOrder Ord>
void AddressableLEDDriver::addClocked(int pin, int clockPin) {
  FastLED.addLeds<Chip, Ord>(_leds, _ledCount, pin, clockPin);
  _added = true;
}

bool AddressableLEDDriver::begin() {
  if (_ledCount <= 0) _ledCount = 1;
  _leds = new CRGB[_ledCount];
  if (!_leds) return false;
  memset(_leds, 0, sizeof(CRGB) * _ledCount);

  switch (_chipset) {
    case CHIP_WS2812:
      if (_colorOrder == ORDER_RGB) addOneWire<WS2812, RGB>(_dataPin);
      else addOneWire<WS2812, GRB>(_dataPin);
      break;
    case CHIP_WS2811:
      if (_colorOrder == ORDER_RGB) addOneWire<WS2811, RGB>(_dataPin);
      else addOneWire<WS2811, GRB>(_dataPin);
      break;
    case CHIP_WS2815:
      if (_colorOrder == ORDER_RGB) addOneWire<WS2815, RGB>(_dataPin);
      else addOneWire<WS2815, GRB>(_dataPin);
      break;
    case CHIP_SK6812:
      if (_colorOrder == ORDER_RGB) addOneWire<SK6812, RGB>(_dataPin);
      else addOneWire<SK6812, GRB>(_dataPin);
      break;
    case CHIP_SK6812_RGBW:
      addOneWire<SK6812, RGBW>(_dataPin);
      break;
    case CHIP_HD107S:
      addClocked<HD107S, BGR>(_dataPin, _clockPin);
      break;
    case CHIP_APA102:
    default:
      if (_colorOrder == ORDER_RGB) addClocked<APA102, RGB>(_dataPin, _clockPin);
      else addClocked<APA102, BGR>(_dataPin, _clockPin);
      break;
  }

  if (!_added) return false;
  applyPowerLimit();
  clear();
  show();
  return true;
}

void AddressableLEDDriver::clear() {
  for (int i = 0; i < _ledCount; ++i) _leds[i] = CRGB(0, 0, 0);
}

void AddressableLEDDriver::setPixel(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < 0 || index >= _ledCount) return;
  if (_reverse) index = _ledCount - 1 - index;
  _leds[index] = CRGB(r, g, b);
}

void AddressableLEDDriver::show() {
  if (!_added) return;
  if (_master < 255) nscale8(_leds, _ledCount, _master);
  FastLED.show();
}

void AddressableLEDDriver::applyPowerLimit() {
  if (_maxMa > 0) FastLED.setMaxPowerInVoltsAndMilliamps(_volts, _maxMa);
}