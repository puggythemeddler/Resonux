#pragma once
#include "audio/AudioAnalyzer.h"
#include "config/ConfigDefs.h"
#include "effects/Effect.h"
#include "led/LEDTypes.h"
#include <stdint.h>

struct StripConfig {
  char    name[16] = "Strip 1";
  int     driverType = DRIVER_ADDRESSABLE;
  int     chipset = CHIP_WS2812;
  int     colorOrder = ORDER_GRB;
  int     dataPin = 48;
  int     clockPin = -1;
  int     ledCount = 60;
  bool    reverse = false;
  int     zoneCount = 1;
  StripZone zones[kMaxStripZones] = {};
  bool    commonAnode = false;
  float   pwmFreqHz = 1000.0f;
  int     effectId = EFFECT_SPECTRUM;
  uint8_t maxBrightness = 255;
  uint8_t minBrightness = 12;
  float   maxCurrentA = 1.0f;
  float   maxVolts = 5.0f;
  uint8_t palette = 0;
  uint8_t startHue = 0;
  float   hueSpeed = 8.0f;
  float   sensitivity = 1.0f;
  float   decay = 0.0f;
  uint8_t targetFps = 60;
};

struct Config {
  char               deviceName[24] = "Music-LED";
  int                micSck = 4;
  int                micWs = 5;
  int                micData = 6;
  AudioAnalyzerConfig audio;
  int                stripCount = 1;
  StripConfig        strips[kMaxStrips];
  uint8_t            masterBrightness = 255;
};