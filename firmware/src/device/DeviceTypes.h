#pragma once
#include <stdint.h>
#include <string.h>

// Universal device model — pure C++, no Arduino. This is the shared vocabulary
// used by the DeviceManager, the web API and the frontend. It describes what a
// detected device CAN do (capabilities) and where it is in the adoption
// pipeline (detected -> identified -> compatible -> safe to connect).
//
// Detection is deliberately separated from activation: nothing here ever
// drives a pin, a bus or an output. Unknown hardware stays in the
// "NeedsInvestigation" state until a human confirms a profile.

namespace dev {

constexpr int kMaxDeviceIdLen  = 40;
constexpr int kMaxDeviceNameLen = 40;
constexpr int kMaxProfileIdLen = 24;
constexpr int kMaxCapFloat     = 1.0f;  // reserved for future confidence fields

// ----------------------------------------------------------------- capabilities
enum Capability : uint32_t {
  CAP_NONE           = 0,
  CAP_AUDIO_INPUT    = 1u << 0,   // provides audio samples to the analyzer
  CAP_AUDIO_OUTPUT   = 1u << 1,   // consumes audio (speaker, amp, soundbar)
  CAP_MICROPHONE     = 1u << 2,
  CAP_LINE_IN        = 1u << 3,
  CAP_USB_AUDIO      = 1u << 4,
  CAP_BLUETOOTH_AUDIO = 1u << 5,
  CAP_NETWORK_AUDIO  = 1u << 6,
  CAP_LED_OUTPUT     = 1u << 7,
  CAP_ADDRESSABLE_LED = 1u << 8,
  CAP_RGB_PWM        = 1u << 9,
  CAP_DMX            = 1u << 10,
  CAP_ARTNET         = 1u << 11,
  CAP_SERIAL         = 1u << 12,
  CAP_I2C            = 1u << 13,
  CAP_SPI            = 1u << 14,
  CAP_DISPLAY        = 1u << 15,
  CAP_TOUCH          = 1u << 16,
  CAP_NETWORK        = 1u << 17,
  CAP_STORAGE        = 1u << 18,
};

constexpr int kCapabilityCount = 19;

struct CapabilityInfo {
  Capability  bit;
  const char* id;    // stable web/JSON id
  const char* label; // human label
};

inline const CapabilityInfo& capabilityAt(int i) {
  static const CapabilityInfo kCaps[kCapabilityCount] = {
      {CAP_AUDIO_INPUT, "audio_input", "Audio input"},
      {CAP_AUDIO_OUTPUT, "audio_output", "Audio output"},
      {CAP_MICROPHONE, "microphone", "Microphone"},
      {CAP_LINE_IN, "line_in", "Line input"},
      {CAP_USB_AUDIO, "usb_audio", "USB audio"},
      {CAP_BLUETOOTH_AUDIO, "bluetooth_audio", "Bluetooth audio"},
      {CAP_NETWORK_AUDIO, "network_audio", "Network audio"},
      {CAP_LED_OUTPUT, "led_output", "LED output"},
      {CAP_ADDRESSABLE_LED, "addressable_led", "Addressable LED"},
      {CAP_RGB_PWM, "rgb_pwm", "RGB PWM"},
      {CAP_DMX, "dmx", "DMX"},
      {CAP_ARTNET, "artnet", "Art-Net"},
      {CAP_SERIAL, "serial", "Serial"},
      {CAP_I2C, "i2c", "I2C"},
      {CAP_SPI, "spi", "SPI"},
      {CAP_DISPLAY, "display", "Display"},
      {CAP_TOUCH, "touch", "Touch"},
      {CAP_NETWORK, "network", "Network"},
      {CAP_STORAGE, "storage", "Storage"},
  };
  return kCaps[i];
}

inline const CapabilityInfo& capabilityByIdent(const char* id) {
  static const CapabilityInfo kNone = {CAP_NONE, "", "unknown"};
  if (!id) return kNone;
  for (int i = 0; i < kCapabilityCount; ++i) {
    if (strcmp(capabilityAt(i).id, id) == 0) return capabilityAt(i);
  }
  return kNone;
}

inline const char* capabilityIdent(Capability bit) {
  for (int i = 0; i < kCapabilityCount; ++i)
    if (capabilityAt(i).bit == bit) return capabilityAt(i).id;
  return "";
}

// ------------------------------------------------------------- connection type
enum ConnectionType : uint8_t {
  CONN_UNKNOWN = 0,
  CONN_WIFI,
  CONN_ETHERNET,
  CONN_BLUETOOTH,
  CONN_USB,
  CONN_JACK_3_5MM,
  CONN_RCA,
  CONN_XLR,
  CONN_SPEAKER_LEVEL,  // amplified speaker output — needs attenuator/DI
  CONN_GPIO,
  CONN_I2C_BUS,
  CONN_SPI_BUS,
  CONN_DMX,
  CONN_SERIAL,
  CONN_ARTNET_UDP,
  CONN_SYNC_UDP,       // Resonux Sync multicast
  CONN_NETWORK,        // RESO_DISCOVER peer (LAN responder)
};

constexpr int kConnectionCount = 17;
constexpr int CONN_COUNT = kConnectionCount;

struct ConnectionInfo {
  ConnectionType kind;
  const char*    id;    // stable web/JSON id
  const char*    label;
};

inline const ConnectionInfo& connectionAt(int i) {
  static const ConnectionInfo kConns[kConnectionCount] = {
      {CONN_WIFI, "wifi", "Wi-Fi"},
      {CONN_ETHERNET, "ethernet", "Ethernet"},
      {CONN_BLUETOOTH, "bluetooth", "Bluetooth"},
      {CONN_USB, "usb", "USB"},
      {CONN_JACK_3_5MM, "jack", "3.5mm jack"},
      {CONN_RCA, "rca", "RCA"},
      {CONN_XLR, "xlr", "XLR"},
      {CONN_SPEAKER_LEVEL, "speaker_level", "Speaker-level"},
      {CONN_GPIO, "gpio", "GPIO"},
      {CONN_I2C_BUS, "i2c", "I2C"},
      {CONN_SPI_BUS, "spi", "SPI"},
      {CONN_DMX, "dmx", "DMX"},
      {CONN_SERIAL, "serial", "Serial"},
      {CONN_ARTNET_UDP, "artnet", "Art-Net UDP"},
      {CONN_SYNC_UDP, "sync", "Sync UDP"},
      {CONN_NETWORK, "network", "Network"},
      {CONN_UNKNOWN, "unknown", "Unknown"},
  };
  return kConns[i];
}

inline const char* connectionIdent(ConnectionType c) {
  for (int i = 0; i < kConnectionCount; ++i)
    if (connectionAt(i).kind == c) return connectionAt(i).id;
  return "unknown";
}

inline const char* connectionLabel(ConnectionType c) {
  for (int i = 0; i < kConnectionCount; ++i)
    if (connectionAt(i).kind == c) return connectionAt(i).label;
  return "Unknown";
}

inline const char* connectionName(ConnectionType c) {
  switch (c) {
    case CONN_WIFI: return "Wi-Fi";
    case CONN_ETHERNET: return "Ethernet";
    case CONN_BLUETOOTH: return "Bluetooth";
    case CONN_USB: return "USB";
    case CONN_JACK_3_5MM: return "3.5mm jack";
    case CONN_RCA: return "RCA";
    case CONN_XLR: return "XLR";
    case CONN_SPEAKER_LEVEL: return "speaker-level";
    case CONN_GPIO: return "GPIO";
    case CONN_I2C_BUS: return "I2C";
    case CONN_SPI_BUS: return "SPI";
    case CONN_DMX: return "DMX";
    case CONN_SERIAL: return "Serial";
    case CONN_ARTNET_UDP: return "Art-Net UDP";
    case CONN_SYNC_UDP: return "Sync UDP";
    default: return "unknown";
  }
}

// ---------------------------------------------------------------- device state
enum DeviceStatus : uint8_t {
  STATUS_DETECTED = 0,        // presence observed, nothing assumed
  STATUS_IDENTIFIED,          // an identity/profile is known
  STATUS_COMPATIBLE,          // profile exists and is compatible
  STATUS_SAFE_TO_CONNECT,     // human confirmed; integration allowed
  STATUS_NEEDS_INVESTIGATION, // signals/levels unknown — never auto-drive
  STATUS_CONFIGURED,          // a binding/config is persisted
};

constexpr int STATUS_COUNT = 6;

inline const char* statusIdent(DeviceStatus s) {
  switch (s) {
    case STATUS_DETECTED: return "detected";
    case STATUS_IDENTIFIED: return "identified";
    case STATUS_COMPATIBLE: return "compatible";
    case STATUS_SAFE_TO_CONNECT: return "safe_to_connect";
    case STATUS_NEEDS_INVESTIGATION: return "needs_investigation";
    case STATUS_CONFIGURED: return "configured";
  }
  return "unknown";
}

inline const char* statusLabel(DeviceStatus s) {
  switch (s) {
    case STATUS_DETECTED: return "Detected";
    case STATUS_IDENTIFIED: return "Identified";
    case STATUS_COMPATIBLE: return "Compatible";
    case STATUS_SAFE_TO_CONNECT: return "Safe to connect";
    case STATUS_NEEDS_INVESTIGATION: return "Needs investigation";
    case STATUS_CONFIGURED: return "Configured";
  }
  return "Unknown";
}

// How a device became known. Discovery is transient; user-confirmed rows are
// persisted and become the trusted baseline across reboots.
enum DiscoverySource : uint8_t {
  SRC_UNKNOWN = 0,
  SRC_LOCAL,      // this controller's own hardware (mic, display, self)
  SRC_SYNC,       // discovered Resonux controller on the sync group
  SRC_NETWORK,    // RESO_DISCOVER responder on the network
  SRC_MANUAL,     // user-configured profile/device
  SRC_PROFILE,    // matched a user-created profile
};

constexpr int SRC_COUNT = 6;

inline const char* sourceIdent(DiscoverySource s) {
  switch (s) {
    case SRC_LOCAL: return "local";
    case SRC_SYNC: return "sync";
    case SRC_NETWORK: return "network";
    case SRC_MANUAL: return "manual";
    case SRC_PROFILE: return "profile";
    default: return "unknown";
  }
}

// ------------------------------------------------------------------ device row
struct DeviceInfo {
  char            id[kMaxDeviceIdLen] = "";
  char            name[kMaxDeviceNameLen] = "";
  char            profileId[kMaxProfileIdLen] = "";
  uint32_t        capabilities = CAP_NONE;
  ConnectionType  connection = CONN_UNKNOWN;
  DeviceStatus    status = STATUS_DETECTED;
  DiscoverySource source = SRC_UNKNOWN;
  bool            persisted = false;   // survives reboot (trusted)
  bool            needsConditioning = false;  // speaker-level / voltage risk
  char            note[80] = "";
  uint32_t        lastSeenMs = 0;
};

inline void deviceClear(DeviceInfo& d) {
  memset(d.id, 0, sizeof(d.id));
  memset(d.name, 0, sizeof(d.name));
  memset(d.profileId, 0, sizeof(d.profileId));
  d.capabilities = CAP_NONE;
  d.connection = CONN_UNKNOWN;
  d.status = STATUS_DETECTED;
  d.source = SRC_UNKNOWN;
  d.persisted = false;
  d.needsConditioning = false;
  memset(d.note, 0, sizeof(d.note));
  d.lastSeenMs = 0;
}

inline bool hasCapability(const DeviceInfo& d, Capability c) {
  return (d.capabilities & c) != 0u;
}

}  // namespace dev