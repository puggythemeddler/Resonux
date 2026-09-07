#pragma once
#include "config/Config.h"
#include <cstdint>

enum FixtureChannelType : uint8_t {
  CH_NONE = 0,
  CH_PAN,
  CH_PAN_FINE,
  CH_TILT,
  CH_TILT_FINE,
  CH_DIMMER,
  CH_RED,
  CH_GREEN,
  CH_BLUE,
  CH_WHITE,
  CH_STROBE,
  CH_GOBO,
  CH_PRISM,
  CH_FOCUS,
  CH_SPEED,
};

struct FixtureProfile {
  const char* name;
  int         channelCount;
  FixtureChannelType channels[20];
  uint16_t    panRange;   // degrees (e.g. 540)
  uint16_t    tiltRange;  // degrees (e.g. 270)
};

static const FixtureProfile kFixtureProfiles[] = {
    // [0] OFF
    {"Off", 0, {}, 0, 0},

    // [1] 8-ch moving head: Pan, PanFine, Tilt, TiltFine, Dimmer, R, G, B
    {"MH-8ch",
     8,
     {CH_PAN, CH_PAN_FINE, CH_TILT, CH_TILT_FINE, CH_DIMMER, CH_RED, CH_GREEN,
      CH_BLUE},
     540,
     270},

    // [2] 16-ch moving head: Pan, PanFine, Tilt, TiltFine, Dimmer, R, G, B,
    //     White, Strobe, Gobo, Prism, Focus, Speed + 3 spare
    {"MH-16ch",
     16,
     {CH_PAN,
      CH_PAN_FINE,
      CH_TILT,
      CH_TILT_FINE,
      CH_DIMMER,
      CH_RED,
      CH_GREEN,
      CH_BLUE,
      CH_WHITE,
      CH_STROBE,
      CH_GOBO,
      CH_PRISM,
      CH_FOCUS,
      CH_SPEED,
      CH_NONE,
      CH_NONE},
     540,
     270},

    // [3] 4-ch LED par: Dimmer, Red, Green, Blue
    {"Par-4ch",
     4,
     {CH_DIMMER, CH_RED, CH_GREEN, CH_BLUE},
     0,
     0},
};

constexpr int kFixtureProfileCount =
    sizeof(kFixtureProfiles) / sizeof(kFixtureProfiles[0]);

inline const FixtureProfile* getFixtureProfile(int id) {
  if (id < 0 || id >= kFixtureProfileCount) return &kFixtureProfiles[0];
  return &kFixtureProfiles[id];
}
