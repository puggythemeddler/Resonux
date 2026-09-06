#include "config/ConfigDefaults.h"
#include <string.h>

static const char kDeviceName[] = "Music-LED";

void configDefaults(Config& cfg) {
  memset(&cfg, 0, sizeof(Config));
  strncpy(cfg.deviceName, kDeviceName, sizeof(cfg.deviceName) - 1);

  cfg.micSck = 4;
  cfg.micWs = 5;
  cfg.micData = 6;
  cfg.masterBrightness = 255;

  AudioAnalyzerConfig& a = cfg.audio;
  a.sampleRate = 44100;
  a.fftSize = 1024;
  a.hopSize = 512;
  a.bandCount = 9;
  a.gain = 1.0f;
  a.normMode = 1;
  a.noiseGate = 0.02f;
  a.smooth = true;
  a.attack = 0.60f;
  a.release = 0.14f;
  a.beatDetect = true;
  a.beatSens = 1.0f;
  a.beatMinGapMs = 250;
  a.beatLowBands = 3;
  a.ampGain = 1.5f;
  a.ampDbFloor = -55.0f;
  a.ampDbCeil = -12.0f;

  static const float kBands[9][2] = {
      {20, 60}, {60, 120}, {120, 250}, {250, 500}, {500, 1000},
      {1000, 2000}, {2000, 4000}, {4000, 8000}, {8000, 16000}};
  for (int b = 0; b < 9; ++b) {
    a.bands[b].loHz = kBands[b][0];
    a.bands[b].hiHz = kBands[b][1];
  }
  const int kGroups[5][2] = {
      {0, 1}, {2, 2}, {3, 4}, {5, 6}, {7, 8}};
  for (int g = 0; g < 5; ++g) {
    a.groupRanges[g][0] = kGroups[g][0];
    a.groupRanges[g][1] = kGroups[g][1];
  }

  cfg.stripCount = 1;
  StripConfig& s = cfg.strips[0];
  strncpy(s.name, "Main", sizeof(s.name) - 1);
  s.driverType = DRIVER_ADDRESSABLE;
  s.chipset = CHIP_WS2812;
  s.colorOrder = ORDER_GRB;
  s.dataPin = 48;
  s.clockPin = -1;
  s.ledCount = 60;
  s.effectId = EFFECT_SPECTRUM;
  s.maxBrightness = 255;
  s.minBrightness = 12;
  s.maxCurrentA = 1.0f;
  s.maxVolts = 5.0f;
  s.targetFps = 60;
  s.sensitivity = 1.0f;
  s.hueSpeed = 8.0f;
}