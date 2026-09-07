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

EOrder AddressableLEDDriver::fastledOrder(int order) {
  switch (order) {
    case ORDER_GRB: return GRB;
    case ORDER_RGB: return RGB;
    case ORDER_BRG: return BRG;
    case ORDER_BGR: return BGR;
    case ORDER_RGBW: return GRB;
    default: return GRB;
  }
}

#define FL_ONE_WIRE_CASE(P)               \
  case P:                                 \
    FastLED.addLeds<Chip, P, Ord>(_leds, _ledCount); \
    return true;

template <template<uint8_t DATA_PIN, EOrder RGB_ORDER> class Chip, EOrder Ord>
bool AddressableLEDDriver::addOneWire(int pin) {
  switch (pin) {
    FL_ONE_WIRE_CASE(1)
    FL_ONE_WIRE_CASE(2)
    FL_ONE_WIRE_CASE(3)
    FL_ONE_WIRE_CASE(5)
    FL_ONE_WIRE_CASE(6)
    FL_ONE_WIRE_CASE(7)
    FL_ONE_WIRE_CASE(8)
    FL_ONE_WIRE_CASE(9)
    FL_ONE_WIRE_CASE(10)
    FL_ONE_WIRE_CASE(11)
    FL_ONE_WIRE_CASE(12)
    FL_ONE_WIRE_CASE(13)
    FL_ONE_WIRE_CASE(14)
    FL_ONE_WIRE_CASE(15)
    FL_ONE_WIRE_CASE(16)
    FL_ONE_WIRE_CASE(17)
    FL_ONE_WIRE_CASE(18)
    FL_ONE_WIRE_CASE(19)
    FL_ONE_WIRE_CASE(20)
    FL_ONE_WIRE_CASE(21)
    FL_ONE_WIRE_CASE(33)
    FL_ONE_WIRE_CASE(34)
    FL_ONE_WIRE_CASE(35)
    FL_ONE_WIRE_CASE(36)
    FL_ONE_WIRE_CASE(37)
    FL_ONE_WIRE_CASE(38)
    FL_ONE_WIRE_CASE(39)
    FL_ONE_WIRE_CASE(40)
    FL_ONE_WIRE_CASE(41)
    FL_ONE_WIRE_CASE(42)
    FL_ONE_WIRE_CASE(47)
    FL_ONE_WIRE_CASE(48)
    default:
      break;
  }
  return false;
}
#undef FL_ONE_WIRE_CASE

#define FL_CLOCKED_CASE(P)                                                    \
  case P:                                                                    \
    switch (clockPin) {                                                      \
      case 4:  FastLED.addLeds<APA102, P, 4, Ord>(_leds, _ledCount); return true; \
      case 8:  FastLED.addLeds<APA102, P, 8, Ord>(_leds, _ledCount); return true; \
      case 10: FastLED.addLeds<APA102, P, 10, Ord>(_leds, _ledCount); return true; \
      case 13: FastLED.addLeds<APA102, P, 13, Ord>(_leds, _ledCount); return true; \
      case 15: FastLED.addLeds<APA102, P, 15, Ord>(_leds, _ledCount); return true; \
      case 18: FastLED.addLeds<APA102, P, 18, Ord>(_leds, _ledCount); return true; \
      case 33: FastLED.addLeds<APA102, P, 33, Ord>(_leds, _ledCount); return true; \
      case 38: FastLED.addLeds<APA102, P, 38, Ord>(_leds, _ledCount); return true; \
      case 47: FastLED.addLeds<APA102, P, 47, Ord>(_leds, _ledCount); return true; \
      case 48: FastLED.addLeds<APA102, P, 48, Ord>(_leds, _ledCount); return true; \
      default: break;                                                        \
    }                                                                        \
    break;

template <EOrder Ord>
bool AddressableLEDDriver::addClocked(int pin, int clockPin) {
  switch (pin) {
    FL_CLOCKED_CASE(1)
    FL_CLOCKED_CASE(2)
    FL_CLOCKED_CASE(3)
    FL_CLOCKED_CASE(5)
    FL_CLOCKED_CASE(6)
    FL_CLOCKED_CASE(8)
    FL_CLOCKED_CASE(9)
    FL_CLOCKED_CASE(10)
    FL_CLOCKED_CASE(11)
    FL_CLOCKED_CASE(13)
    FL_CLOCKED_CASE(14)
    FL_CLOCKED_CASE(15)
    FL_CLOCKED_CASE(16)
    FL_CLOCKED_CASE(17)
    FL_CLOCKED_CASE(18)
    FL_CLOCKED_CASE(19)
    FL_CLOCKED_CASE(21)
    FL_CLOCKED_CASE(33)
    FL_CLOCKED_CASE(34)
    FL_CLOCKED_CASE(35)
    FL_CLOCKED_CASE(37)
    FL_CLOCKED_CASE(38)
    FL_CLOCKED_CASE(39)
    FL_CLOCKED_CASE(40)
    FL_CLOCKED_CASE(41)
    FL_CLOCKED_CASE(42)
    FL_CLOCKED_CASE(48)
    default:
      break;
  }
  return false;
}
#undef FL_CLOCKED_CASE

bool AddressableLEDDriver::begin() {
  if (_ledCount <= 0) _ledCount = 1;
  _leds = new CRGB[_ledCount];
  if (!_leds) return false;
  memset(_leds, 0, sizeof(CRGB) * _ledCount);

  bool ok = false;
  switch (_chipset) {
    case CHIP_WS2812:
    case CHIP_WS2815:
      switch (fastledOrder(_colorOrder)) {
        case RGB: ok = addOneWire<WS2812, RGB>(_dataPin); break;
        case BRG: ok = addOneWire<WS2812, BRG>(_dataPin); break;
        case BGR: ok = addOneWire<WS2812, BGR>(_dataPin); break;
        case GRB:
        default: ok = addOneWire<WS2812, GRB>(_dataPin); break;
      }
      break;
    case CHIP_WS2811:
      switch (fastledOrder(_colorOrder)) {
        case RGB: ok = addOneWire<WS2811, RGB>(_dataPin); break;
        case BRG: ok = addOneWire<WS2811, BRG>(_dataPin); break;
        case BGR: ok = addOneWire<WS2811, BGR>(_dataPin); break;
        case GRB:
        default: ok = addOneWire<WS2811, GRB>(_dataPin); break;
      }
      break;
    case CHIP_SK6812:
    case CHIP_SK6812_RGBW:
      switch (fastledOrder(_colorOrder)) {
        case RGB: ok = addOneWire<SK6812, RGB>(_dataPin); break;
        case BRG: ok = addOneWire<SK6812, BRG>(_dataPin); break;
        case BGR: ok = addOneWire<SK6812, BGR>(_dataPin); break;
        case GRB:
        default: ok = addOneWire<SK6812, GRB>(_dataPin); break;
      }
      break;
    case CHIP_APA102:
    case CHIP_HD107S:
      switch (fastledOrder(_colorOrder)) {
        case RGB: ok = addClocked<RGB>(_dataPin, _clockPin); break;
        case BRG: ok = addClocked<BRG>(_dataPin, _clockPin); break;
        case GRB: ok = addClocked<GRB>(_dataPin, _clockPin); break;
        case BGR:
        default: ok = addClocked<BGR>(_dataPin, _clockPin); break;
      }
      break;
    default:
      break;
  }
  if (!ok) return false;

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
  if (!_leds) return;
  if (_master < 255) nscale8(_leds, _ledCount, _master);
  FastLED.show();
}

void AddressableLEDDriver::applyPowerLimit() {
  if (_maxMa > 0) FastLED.setMaxPowerInVoltsAndMilliamps(_volts, _maxMa);
}