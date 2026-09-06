#pragma once
#include <stdint.h>

class LEDDriver {
public:
  virtual ~LEDDriver() {}

  virtual bool begin() = 0;
  virtual void clear() = 0;
  virtual void setPixel(int index, uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void show() = 0;
  virtual int segmentCount() const = 0;
  virtual bool isAddressable() const = 0;
  virtual uint8_t channelCount() const = 0;
  virtual void setMasterBrightness(uint8_t b) = 0;
  virtual void setPowerLimit(float maxA, float volts) = 0;
  virtual const char* driverName() const = 0;
};