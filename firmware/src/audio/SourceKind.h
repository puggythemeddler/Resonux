#pragma once
#include <string.h>

// Audio source taxonomy. Resonux runs one active AudioSource feeding the
// analyzer; this enum identifies which kind that is. Several kinds are only
// reachable through external devices (USB audio, Bluetooth, network) and are
// listed here so the UI/API surface is stable even though no local hardware
// exists yet. SOURCE_MIC is the historical default (INMP441).

enum SourceKind : int {
  SOURCE_NONE      = 0,  // silence / no input; effects idle
  SOURCE_MIC       = 1,  // onboard I2S microphone (INMP441)
  SOURCE_LINE_IN   = 2,  // external line-level input via a device
  SOURCE_USB_AUDIO = 3,  // USB audio interface (device-provided)
  SOURCE_BLUETOOTH = 4,  // Bluetooth A2DP device
  SOURCE_NETWORK   = 5,  // network/streaming audio device
  SOURCE_DEVICE    = 6,  // any device-provided audio source
  SOURCE_TEST      = 7,  // built-in tone generator (no hardware required)
  SOURCE_COUNT     = 8,
};

inline const char* sourceKindIdent(SourceKind k) {
  switch (k) {
    case SOURCE_NONE: return "none";
    case SOURCE_MIC: return "mic";
    case SOURCE_LINE_IN: return "line_in";
    case SOURCE_USB_AUDIO: return "usb_audio";
    case SOURCE_BLUETOOTH: return "bluetooth";
    case SOURCE_NETWORK: return "network";
    case SOURCE_DEVICE: return "device";
    case SOURCE_TEST: return "test";
    case SOURCE_COUNT: return "none";
  }
  return "none";
}

inline const char* sourceKindLabel(SourceKind k) {
  switch (k) {
    case SOURCE_NONE: return "No input";
    case SOURCE_MIC: return "Microphone (INMP441)";
    case SOURCE_LINE_IN: return "Line input";
    case SOURCE_USB_AUDIO: return "USB audio";
    case SOURCE_BLUETOOTH: return "Bluetooth audio";
    case SOURCE_NETWORK: return "Network audio";
    case SOURCE_DEVICE: return "Device audio";
    case SOURCE_TEST: return "Test tone";
    case SOURCE_COUNT: return "No input";
  }
  return "Unknown";
}

inline SourceKind sourceKindFromIdent(const char* ident) {
  if (!ident) return SOURCE_NONE;
  for (int k = SOURCE_NONE; k < SOURCE_COUNT; ++k)
    if (strcmp(sourceKindIdent((SourceKind)k), ident) == 0)
      return (SourceKind)k;
  return SOURCE_NONE;
}

// Kinds this controller can host natively *today*; everything else requires an
// external device to have been detected and configured first.
inline bool sourceIsLocal(SourceKind k) {
  return k == SOURCE_MIC || k == SOURCE_TEST || k == SOURCE_NONE;
}