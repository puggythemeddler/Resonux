#pragma once
#include "audio/SourceKind.h"
#include "device/DeviceTypes.h"
#include <stdint.h>
#include <string.h>

// Data-driven device profiles. A profile is pure metadata describing what a
// device is, what it can do, how it connects, and what must be true before it
// is safe to integrate. The firmware NEVER hard-codes a specific device ("if
// it is AM-006 then..."); instead a profile describes a class of devices and
// the first real-world device validates the *profile*, not the code.
//
// Profiles are pure C++ so the model, matching and validation are host
// unit-testable. JSON translation lives in DeviceManager/DeviceStore.

namespace dev {

constexpr int kMaxConnections  = 4;
constexpr int kMaxAudioSources = 4;
constexpr int kMaxLedInterfaces = 4;
constexpr int kMaxSafetyNotes  = 4;
constexpr int kMaxNameLen      = 32;
constexpr int kMaxProtoLen     = 24;

struct DeviceProfile {
  char            id[kMaxProfileIdLen] = "";
  char            manufacturer[kMaxNameLen] = "";
  char            model[kMaxNameLen] = "";
  char            deviceType[kMaxNameLen] = "";
  uint32_t        capabilities = CAP_NONE;
  ConnectionType  connections[kMaxConnections];
  uint8_t         connectionCount = 0;
  char            protocols[kMaxConnections][kMaxProtoLen];  // e.g. "artnet","i2s","analog"
  uint8_t         protocolCount = 0;
  int             audioSources[kMaxAudioSources];  // dev::SourceKind (-1 = none / unknown)
  uint8_t         audioSourceCount = 0;
  const char*     ledInterfaces[kMaxLedInterfaces];
  uint8_t         ledInterfaceCount = 0;
  const char*     safetyNotes[kMaxSafetyNotes];
  uint8_t         safetyNoteCount = 0;
  bool            needsConditioning = false;  // e.g. speaker-level audio, voltage risk
  bool            builtin = false;
};

inline bool profileHasConnection(const DeviceProfile& p, ConnectionType c) {
  for (int i = 0; i < p.connectionCount; ++i)
    if (p.connections[i] == c) return true;
  return false;
}

inline void profileAddConnection(DeviceProfile& p, ConnectionType c) {
  if (p.connectionCount >= kMaxConnections) return;
  p.connections[p.connectionCount++] = c;
}

inline void profileAddProtocol(DeviceProfile& p, const char* proto) {
  if (!proto || p.protocolCount >= kMaxConnections) return;
  strncpy(p.protocols[p.protocolCount], proto, sizeof(p.protocols[0]) - 1);
  p.protocols[p.protocolCount][sizeof(p.protocols[0]) - 1] = '\0';
  p.protocolCount++;
}

// Built-in profile table. AM-006 is intentionally just another profile row —
// if that specific unit ever grows its own entry it stays additive, and the
// *logic* keeps operating on capabilities, never on the name.
struct BuiltinProfileEntry {
  const char*           id;
  const char*           manufacturer;
  const char*           model;
  const char*           deviceType;
  uint32_t              capabilities;
  const ConnectionType* connections;
  int                   connectionCount;
  const char* const*    protocols;
  int                   protocolCount;
  const int*            audioSources;
  int                   audioSourceCount;
  const char* const*    ledInterfaces;
  int                   ledInterfaceCount;
  const char* const*    safetyNotes;
  int                   safetyNoteCount;
  bool                  needsConditioning;
};

// ------------------------------------------------------------------- builtins
inline const BuiltinProfileEntry* builtinProfileAt(int i);

namespace detail {
inline const ConnectionType kOwnConn[] = {CONN_GPIO, CONN_SYNC_UDP, CONN_ARTNET_UDP};
inline const char* const kOwnProto[] = {"i2s", "neopixel", "sync", "artnet"};
inline const int kOwnAudio[] = {SOURCE_DEVICE};  // mic is local; sources reserved for external
inline const char* const kOwnLed[] = {"neopixel", "rgb-pwm"};
inline const char* const kOwnSafety[] = {
    "Outputs drive addressable LED strips — match voltage (5V) and polarity.",
    "Never attach mains/line-voltage signals to GPIO pins."};
inline const ConnectionType kAm006Conn[] = {CONN_JACK_3_5MM, CONN_RCA, CONN_SPEAKER_LEVEL};
inline const char* const kAm006Proto[] = {"analog"};
inline const int kAm006Audio[] = {SOURCE_NONE};
inline const char* const kAm006Led[] = {};
inline const char* const kAm006Safety[] = {
    "Speaker-level output: amplified, can exceed 1V — MUST be attenuated/DI'd "
    "before any line-IN input.",
    "Do NOT connect speaker output directly to a microphone/line input.",
    "Voltage, level and pinout must be verified manually before integration."};
inline const ConnectionType kMicConn[] = {CONN_GPIO};
inline const char* const kMicProto[] = {"i2s"};
inline const int kMicAudio[] = {SOURCE_MIC};
inline const char* const kMicSafety[] = {"INMP441 module, 3.3V only."};
inline const ConnectionType kMicLedOnboard[] = {CONN_GPIO};
inline const char* const kMicLedOnboardProto[] = {"neopixel", "rgb-pwm"};
inline const int kMicLedOnboardAudio[] = {SOURCE_NONE};
inline const ConnectionType kUsbAudioConn[] = {CONN_USB};
inline const char* const kUsbAudioProto[] = {"usb-audio"};
inline const int kUsbAudioAudio[] = {SOURCE_USB_AUDIO};
inline const ConnectionType kBtAudioConn[] = {CONN_BLUETOOTH};
inline const char* const kBtAudioProto[] = {"a2dp", "avrcp"};
inline const int kBtAudioAudio[] = {SOURCE_BLUETOOTH};
inline const char* const kBtAudioSafety[] = {
    "Bluetooth pairing must be completed manually; signal level is device "
    "controlled."};
inline const ConnectionType kNetAudioConn[] = {CONN_WIFI, CONN_ETHERNET};
inline const char* const kNetAudioProto[] = {"airplay", "dlna", "raw-udp", "stream"};
inline const int kNetAudioAudio[] = {SOURCE_NETWORK};
inline const ConnectionType kDisplayConn[] = {CONN_SPI_BUS, CONN_GPIO};
inline const char* const kDisplayProto[] = {"spi", "lvgl"};
inline const int kDisplayAudio[] = {SOURCE_NONE};
inline const char* const kDisplayLed[] = {"spi-lcd"};
inline const char* const kDisplaySafety[] = {"Drives 3.3V SPI panel — verify backlight pin rating."};
inline const ConnectionType kDmxCtrlConn[] = {CONN_DMX, CONN_ARTNET_UDP};
inline const char* const kDmxCtrlProto[] = {"dmx", "artnet"};
inline const int kDmxCtrlAudio[] = {SOURCE_NONE};
inline const char* const kDmxCtrlLed[] = {"dmx"};
inline const char* const kDmxCtrlSafety[] = {
    "DMX devices are addressed via universe/channel — never energize unknown "
    "fixtures.",
    "DMX line is 5V differential (RS-485); verify pinout and termination."};
}  // namespace detail

inline const BuiltinProfileEntry* builtinProfileAt(int i) {
  static const BuiltinProfileEntry kProfiles[] = {
      {"resonux", "Resonux", "Universal Controller", "lighting-controller",
       dev::CAP_NETWORK | dev::CAP_ARTNET | dev::CAP_LED_OUTPUT |
           dev::CAP_ADDRESSABLE_LED | dev::CAP_RGB_PWM | dev::CAP_MICROPHONE |
           dev::CAP_AUDIO_INPUT | dev::CAP_SPI | dev::CAP_STORAGE,
       detail::kOwnConn, 3, detail::kOwnProto, 4, detail::kOwnAudio, 1,
       detail::kOwnLed, 2, detail::kOwnSafety, 2, false},
      {"am006", "(validation unit)", "AM-006", "multimedia-speaker",
       dev::CAP_AUDIO_OUTPUT | dev::CAP_AUDIO_INPUT,
       detail::kAm006Conn, 3, detail::kAm006Proto, 1, detail::kAm006Audio, 1,
       detail::kAm006Led, 0, detail::kAm006Safety, 3, true},
      {"inmp441_mic", "INMP441", "Omnidirectional I2S Microphone", "microphone",
       dev::CAP_MICROPHONE | dev::CAP_AUDIO_INPUT,
       detail::kMicConn, 1, detail::kMicProto, 1, detail::kMicAudio, 1,
       nullptr, 0, detail::kMicSafety, 1, false},
      {"led_onboard", "(integrated)", "LED Output", "led-driver",
       dev::CAP_LED_OUTPUT | dev::CAP_ADDRESSABLE_LED | dev::CAP_RGB_PWM,
       detail::kMicLedOnboard, 1, detail::kMicLedOnboardProto, 2,
       detail::kMicLedOnboardAudio, 1, nullptr, 0, nullptr, 0, false},
      {"usb_audio", "Generic", "USB Audio Interface", "audio-interface",
       dev::CAP_USB_AUDIO | dev::CAP_AUDIO_INPUT | dev::CAP_AUDIO_OUTPUT,
       detail::kUsbAudioConn, 1, detail::kUsbAudioProto, 1,
       detail::kUsbAudioAudio, 1, nullptr, 0, nullptr, 0, false},
      {"bluetooth_speaker", "Generic", "Bluetooth Speaker", "speaker",
       dev::CAP_BLUETOOTH_AUDIO | dev::CAP_AUDIO_INPUT,
       detail::kBtAudioConn, 1, detail::kBtAudioProto, 2,
       detail::kBtAudioAudio, 1, nullptr, 0, detail::kBtAudioSafety, 1, false},
      {"network_audio", "Generic", "Network Streamer", "network-audio",
       dev::CAP_NETWORK_AUDIO | dev::CAP_AUDIO_INPUT | dev::CAP_NETWORK,
       detail::kNetAudioConn, 2, detail::kNetAudioProto, 4,
       detail::kNetAudioAudio, 1, nullptr, 0, nullptr, 0, false},
      {"touch_display", "(integrated)", "Touch Display", "display",
       dev::CAP_DISPLAY | dev::CAP_TOUCH | dev::CAP_SPI,
       detail::kDisplayConn, 2, detail::kDisplayProto, 2,
       detail::kDisplayAudio, 1, detail::kDisplayLed, 1,
       detail::kDisplaySafety, 1, false},
      {"dmx_fixture", "Generic", "DMX Fixture", "lighting-fixture",
       dev::CAP_DMX | dev::CAP_ARTNET | dev::CAP_LED_OUTPUT,
       detail::kDmxCtrlConn, 2, detail::kDmxCtrlProto, 2,
       detail::kDmxCtrlAudio, 1, detail::kDmxCtrlLed, 1,
       detail::kDmxCtrlSafety, 2, false},
  };
  static const int kCount = (int)(sizeof(kProfiles) / sizeof(kProfiles[0]));
  if (i < 0 || i >= kCount) return nullptr;
  return &kProfiles[i];
}

inline int builtinProfileCount() {
  static const int kCount = 9;
  return kCount;
}

// ------------------------------------------------------------------ validation
// Pure shape/range validation that also runs at JSON-decode time so malformed
// user profiles are rejected before they ever reach the registry.
inline bool validateProfileShape(const char* id, uint32_t capabilities) {
  if (!id || !id[0]) return false;
  if (strlen(id) >= kMaxProfileIdLen) return false;
  if (capabilities == CAP_NONE) return false;
  for (int i = 0; i < kCapabilityCount; ++i)
    if (capabilityAt(i).bit == capabilities) return true;
  // allow arbitrary multi-bit combination, not just a single bit
  return true;
}

// Materialize a BuiltinProfileEntry into a DeviceProfile.
inline void builtinToProfile(const BuiltinProfileEntry& e, DeviceProfile& out) {
  DeviceProfile p;
  strncpy(p.id, e.id, sizeof(p.id) - 1);
  strncpy(p.manufacturer, e.manufacturer, sizeof(p.manufacturer) - 1);
  strncpy(p.model, e.model, sizeof(p.model) - 1);
  strncpy(p.deviceType, e.deviceType, sizeof(p.deviceType) - 1);
  p.capabilities = e.capabilities;
  for (int i = 0; i < e.connectionCount && i < kMaxConnections; ++i)
    p.connections[p.connectionCount++] = e.connections[i];
  for (int i = 0; i < e.protocolCount && i < kMaxConnections; ++i)
    profileAddProtocol(p, e.protocols[i]);
  for (int i = 0; i < e.audioSourceCount && i < kMaxAudioSources; ++i)
    p.audioSources[p.audioSourceCount++] = e.audioSources[i];
  for (int i = 0; i < e.ledInterfaceCount && i < kMaxLedInterfaces; ++i)
    p.ledInterfaces[p.ledInterfaceCount++] = e.ledInterfaces[i];
  for (int i = 0; i < e.safetyNoteCount && i < kMaxSafetyNotes; ++i)
    p.safetyNotes[p.safetyNoteCount++] = e.safetyNotes[i];
  p.needsConditioning = e.needsConditioning;
  p.builtin = true;
  out = p;
}

inline DeviceProfile profileById(const char* id) {
  DeviceProfile out;
  if (id) {
    for (int i = 0; i < builtinProfileCount(); ++i) {
      const BuiltinProfileEntry* e = builtinProfileAt(i);
      if (e && strcmp(e->id, id) == 0) {
        builtinToProfile(*e, out);
        return out;
      }
    }
  }
  DeviceProfile none;
  return none;
}

// ----------------------------------------------------------------- classifying
// Given a detected device (observed presence/connection only), work out how far
// it has progressed: does a profile match, is it compatible, and is it safe?
struct DeviceMatch {
  char        profileId[kMaxProfileIdLen] = "";
  bool        known = false;        // a profile exists for this device
  bool        compatible = false;   // profile + observed connection line up
  bool        safeToConnect = false;// no un-verified electrical risks remain
  DeviceStatus status = STATUS_NEEDS_INVESTIGATION;
};

inline DeviceMatch classify(const DeviceInfo& d) {
  DeviceMatch m;
  m.status = STATUS_NEEDS_INVESTIGATION;
  if (d.status == STATUS_CONFIGURED || d.status == STATUS_SAFE_TO_CONNECT) {
    m.known = true;
    m.compatible = true;
    m.safeToConnect = true;
    m.status = d.status;
    return m;
  }
  strncpy(m.profileId, d.profileId, sizeof(m.profileId) - 1);

  if (!m.profileId[0]) {
    // No identity at all: everything beyond "presence observed" is unknown,
    // so never auto-drive it.
    m.status = d.needsConditioning ? STATUS_NEEDS_INVESTIGATION
                                   : STATUS_DETECTED;
    return m;
  }

  DeviceProfile p = profileById(m.profileId);
  m.known = true;
  if (p.id[0]) {
    m.compatible = true;
    if (!p.needsConditioning) {
      m.safeToConnect = true;
      m.status = STATUS_SAFE_TO_CONNECT;
    } else {
      // Profile known + compatible, but the electrical interface carries a
      // real risk (speaker-level, voltage) — human verification required.
      m.safeToConnect = false;
      m.status = STATUS_COMPATIBLE;
    }
  } else {
    // A profile id was referenced but it doesn't exist — treat as unknown.
    m.status = STATUS_NEEDS_INVESTIGATION;
  }
  return m;
}

}  // namespace dev