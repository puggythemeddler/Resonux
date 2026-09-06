#include "led/factory/LEDDriverFactory.h"
#include "led/addressable/AddressableLEDDriver.h"
#include "led/analog/AnalogRGBDriver.h"
#include "led/single/SingleColorDriver.h"

LEDDriver* createLEDDriver(const StripConfig& cfg) {
  switch (cfg.driverType) {
    case DRIVER_ANALOG_RGB:
      return new AnalogRGBDriver(cfg.zones, cfg.zoneCount, cfg.commonAnode,
                                 cfg.maxCurrentA, cfg.maxVolts, cfg.pwmFreqHz,
                                 cfg.maxBrightness);
    case DRIVER_SINGLE_COLOR:
      return new SingleColorDriver(cfg.zones, cfg.zoneCount, cfg.maxCurrentA,
                                   cfg.maxVolts, cfg.pwmFreqHz,
                                   cfg.maxBrightness);
    case DRIVER_ADDRESSABLE:
    default:
      return new AddressableLEDDriver(cfg.dataPin, cfg.clockPin, cfg.ledCount,
                                      cfg.chipset, cfg.colorOrder, cfg.reverse,
                                      cfg.maxBrightness, cfg.maxCurrentA,
                                      cfg.maxVolts);
  }
}