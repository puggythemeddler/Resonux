#pragma once
#include "config/ConfigDefs.h"

enum DriverType {
  DRIVER_ADDRESSABLE = 0,
  DRIVER_ANALOG_RGB,
  DRIVER_SINGLE_COLOR,
  DRIVER_TYPE_COUNT
};

enum AddressableChipset {
  CHIP_WS2812 = 0,
  CHIP_WS2811,
  CHIP_WS2815,
  CHIP_SK6812,
  CHIP_SK6812_RGBW,
  CHIP_APA102,
  CHIP_HD107S,
  CHIP_COUNT
};

enum ColorOrder {
  ORDER_GRB = 0,
  ORDER_RGB,
  ORDER_BRG,
  ORDER_RGBW,
  ORDER_BGR,
  ORDER_COUNT
};

struct StripZone {
  int   pins[3] = {-1, -1, -1};
  int   nPins = 1;
  float pos01 = 0.0f;
  int   band = 0;
};