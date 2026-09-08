#pragma once
#include "audio/AudioAnalyzer.h"
#include "config/ConfigDefs.h"
#include "display/DisplayTypes.h"
#include "effects/Effect.h"
#include "led/LEDTypes.h"
#include "theme/Theme.h"
#include <stdint.h>

struct StripConfig {
  char    name[16] = "Strip 1";
  char    themeId[Themes::kMaxIdLen] = "";  // "" => follow global theme
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

enum FixtureProfileId : int {
  FIXTURE_OFF          = 0,
  FIXTURE_MOVING_HEAD_8  = 1,
  FIXTURE_MOVING_HEAD_16 = 2,
  FIXTURE_LED_PAR_4      = 3,
  FIXTURE_COUNT          = 4
};

struct FixtureConfig {
  int  profileId   = 0;
  int  dmxAddress  = 1;
  int  count       = 0;
};

struct ArtNetConfig {
  bool    enabled        = false;
  char    ssid[33]       = "";
  char    password[65]   = "";
  bool    useDhcp        = true;
  uint8_t staticIp[4]    = {};
  uint8_t staticMask[4]  = {};
  uint8_t staticGw[4]    = {};
  uint8_t universe       = 0;
  bool    audioReactive  = true;
  float   panSpeed       = 0.5f;
  float   tiltSpeed      = 0.5f;
  float   colorSensitivity = 1.0f;
};

enum NetworkMode : int {
  NET_AP_STA_FALLBACK = 0,  // try STA, fall back to AP hotspot
  NET_STA_ONLY        = 1,
  NET_AP_ONLY         = 2,
};

struct NetworkConfig {
  bool     enabled     = true;
  int      mode        = NET_AP_STA_FALLBACK;
  char     apSsid[24]  = "Resonux";
  char     apPassword[33] = "";
  char     staSsid[33] = "";
  char     staPassword[65] = "";
};

struct Config {
  char               deviceName[24] = "Music-LED";
  char               themeId[Themes::kMaxIdLen] = "classic";
  int                micSck = 4;
  int                micWs = 5;
  int                micData = 6;
  AudioAnalyzerConfig audio;
  int                stripCount = 1;
  StripConfig        strips[kMaxStrips];
  uint8_t            masterBrightness = 255;
  NetworkConfig      net;
  ArtNetConfig       artnet;
  DisplayConfig      display;
  int                fixtureCount = 0;
  FixtureConfig      fixtures[kMaxFixtures];
};