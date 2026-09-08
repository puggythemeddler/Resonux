#pragma once
#include "audio/AudioFrame.h"
#include "config/ConfigDefs.h"
#include <stdint.h>
#include <string.h>

// Wire format for master -> slave AudioFrame multicast (Phase 9).
// Pure C++ (no Arduino/FreeRTOS deps) so the host-side unit tests can cover
// pack/unpack/validation.

namespace syncpkt {

constexpr uint32_t kSyncMagic       = 0x53584E52u;  // 'RNXS' little-endian
constexpr uint16_t kSyncVersion     = 1;
constexpr uint16_t kSyncDefaultPort = 9769;
constexpr char     kSyncDefaultGroup[] = "239.255.42.9";

#pragma pack(push, 1)
struct Packet {
  uint32_t magic = 0;
  uint16_t version = 0;
  uint16_t flags = 0;        // reserved, keep 0
  uint32_t seq = 0;          // master frame counter
  uint32_t masterTime = 0;   // AudioFrame.timeMs at analysis (master millis)
  uint8_t  bandCount = 0;
  uint8_t  beat = 0;         // bool stored as byte
  uint8_t  pad[2] = {0, 0};
  float    bands[kMaxBands];
  float    peaks[kMaxBands];
  float    amplitude;
  float    bass;
  float    lowMid;
  float    mid;
  float    highMid;
  float    treble;
  float    beatStrength;
};
#pragma pack(pop)

inline uint16_t packetBytes() { return sizeof(Packet); }

// bytes == number of bytes actually read off the wire.
inline bool validPacket(const Packet& p, size_t bytes) {
  if (bytes < sizeof(Packet)) return false;
  if (p.magic != kSyncMagic || p.version != kSyncVersion) return false;
  return p.bandCount <= kMaxBands;
}

inline void packPacket(Packet& p, const AudioFrame& f, uint32_t seq) {
  memset(&p, 0, sizeof(p));
  p.magic = kSyncMagic;
  p.version = kSyncVersion;
  p.flags = 0;
  p.seq = seq;
  p.masterTime = f.timeMs;
  p.bandCount = f.bandCount;
  p.beat = f.beat ? 1 : 0;
  for (int i = 0; i < kMaxBands; ++i) {
    p.bands[i] = f.bands[i];
    p.peaks[i] = f.peaks[i];
  }
  p.amplitude = f.amplitude;
  p.bass = f.bass;
  p.lowMid = f.lowMid;
  p.mid = f.mid;
  p.highMid = f.highMid;
  p.treble = f.treble;
  p.beatStrength = f.beatStrength;
}

// The caller applies clock-offset compensation to f.timeMs afterwards
// (SyncClock::toLocal) so slave-side effects see master-consistent timing.
inline void unpackPacket(AudioFrame& f, const Packet& p) {
  memset(&f, 0, sizeof(f));
  f.timeMs = p.masterTime;
  f.bandCount = p.bandCount;
  f.beat = p.beat != 0;
  for (int i = 0; i < kMaxBands; ++i) {
    f.bands[i] = p.bands[i];
    f.peaks[i] = p.peaks[i];
  }
  f.amplitude = p.amplitude;
  f.bass = p.bass;
  f.lowMid = p.lowMid;
  f.mid = p.mid;
  f.highMid = p.highMid;
  f.treble = p.treble;
  f.beatStrength = p.beatStrength;
}

}  // namespace syncpkt