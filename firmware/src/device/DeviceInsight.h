#pragma once
#include "audio/SourceKind.h"
#include "device/DeviceProfile.h"  // classify(), profileById()
#include <stdint.h>

// Derived insight about a device — pure C++ and host-testable.
//
// * Confidence is a tri-layer, honest breakdown: identity (do we know what it
//   is?), capability (do we know what it can do?), integration (can we safely
//   use it right now?). A discovered device with no profile scores low on
//   identity/confidence even though it was "seen" on the network.
// * Integration recommendations translate capabilities into possible Resonux
//   integrations WITHOUT claiming automatic compatibility. The `safe` flag is
//   deliberately conservative: any conditioning/verification requirement keeps
//   the recommendation visible but unsafe until a human confirms.

namespace dev {

struct Confidence {
  uint8_t identity = 0;
  uint8_t capability = 0;
  uint8_t integration = 0;
};

inline bool isUserConfirmed(const DeviceInfo& d) {
  return d.status == STATUS_CONFIGURED || d.status == STATUS_SAFE_TO_CONNECT;
}

inline bool hasValidProfile(const DeviceInfo& d) {
  return d.profileId[0] && profileById(d.profileId).id[0];
}

inline Confidence deviceConfidence(const DeviceInfo& d) {
  Confidence c;
  if (isUserConfirmed(d)) {
    c.identity = 100;
    c.capability = 100;
    c.integration = d.needsConditioning ? 60 : 100;
  } else if (hasValidProfile(d)) {
    c.identity = 100;  // a profile for this device class exists
    c.capability = 100;
    c.integration = d.needsConditioning ? 60 : 90;
  } else if (d.source == SRC_MANUAL || d.source == SRC_PROFILE) {
    c.identity = 85;   // user-asserted, unverified against a real profile
    c.capability = 85;
    c.integration = d.needsConditioning ? 50 : 95;
  } else {
    c.identity = 40;   // observed, not identified
    c.capability = 40;
    c.integration = 0;
  }
  return c;
}

// ----------------------------------------------------- integration insights
struct IntegrationRec {
  const char* kind;     // stable id: "audio_source", "led_output", "artnet", ...
  const char* title;    // short label
  const char* route;    // how Resonux could talk to it today
  bool        recommended;
  bool        safe;     // no un-verified electrical work required
};

inline IntegrationRec integrationAt(int i) {
  static const IntegrationRec kRecs[] = {
      {"audio_source", "Audio analysis input",
       "Feed samples into the analyzer via an AudioSource adapter", false, false},
      {"audio_output", "Playback target",
       "Route Resonux audio output to the device (A2DP/analog)", false, false},
      {"led_output", "Lighting output",
       "Drive the device as a lighting output through a known LED/DMX driver",
       false, false},
      {"artnet", "Art-Net receiver",
       "Send DMX universes to the device over Wi-Fi (Art-Net)", false, false},
      {"sync_peer", "Resonux sync peer",
       "Join a beat-locked multi-controller audio group (multicast sync)",
       false, false},
  };
  return kRecs[i];
}

// Maps the device's capabilities to candidate integrations. Never claims the
// integration is wired up — `recommended` only means the capability exists and
// Resonux has a protocol path for it.
inline int recommendIntegrations(const DeviceInfo& d, IntegrationRec* out,
                                 int maxOut) {
  int n = 0;
  auto add = [&](int i, bool recommended) {
    if (n >= maxOut) return;
    IntegrationRec r = integrationAt(i);
    r.recommended = recommended;
    r.safe = recommended && !d.needsConditioning;
    out[n++] = r;
  };
  add(0, (d.capabilities & CAP_AUDIO_INPUT) != 0u);
  add(1, (d.capabilities & CAP_AUDIO_OUTPUT) != 0u);
  add(2, (d.capabilities & CAP_LED_OUTPUT) != 0u);
  add(3, (d.capabilities & CAP_ARTNET) != 0u);
  add(4, (d.capabilities & CAP_NETWORK) != 0u);
  return n;
}

// ---------------------------------------------------- audio source enablement
// Whether a device row with `caps` would make audio source `k` selectable.
// In the UI this turns "reserved" sources into available ones as soon as a
// trusted device with the matching capability exists.
inline bool deviceEnablesSource(uint32_t caps, SourceKind k) {
  switch (k) {
    case SOURCE_MIC:       return (caps & CAP_MICROPHONE) != 0u;
    case SOURCE_LINE_IN:   return (caps & CAP_LINE_IN) != 0u;
    case SOURCE_USB_AUDIO: return (caps & CAP_USB_AUDIO) != 0u;
    case SOURCE_BLUETOOTH: return (caps & CAP_BLUETOOTH_AUDIO) != 0u;
    case SOURCE_NETWORK:   return (caps & CAP_NETWORK_AUDIO) != 0u;
    case SOURCE_DEVICE:    return (caps & CAP_AUDIO_INPUT) != 0u;
    default:               return false;
  }
}

// Status for a manually declared device: the user explicitly confirmed the
// capabilities, so a clean declaration is safe to connect; one flagged as
// requiring conditioning stays at "compatible" until levels are verified.
inline DeviceStatus manualDeclareStatus(bool wantsConditioning) {
  return wantsConditioning ? STATUS_COMPATIBLE : STATUS_SAFE_TO_CONNECT;
}

}  // namespace dev