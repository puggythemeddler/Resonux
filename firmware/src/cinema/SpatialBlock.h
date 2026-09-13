#pragma once
#include <stdint.h>
#include <string.h>

// Optional spatial extension to the SceneFrame wire codec (room-mapping
// protocol, Option A). A companion that has per-pixel screen data appends a
// length-tagged block after the base Frame and sets SFLAG_SPATIAL_BLOCK in
// Frame.flags:
//
//   [base 28-byte Frame] [ 'S' 'P' blockLen focusX focusY ...pairs... ]
//
// Receivers that predate the extension still accept the base packet because
// validFrame() tolerates over-long payloads; the block simply reads as
// trailing bytes to them. Length-tagging means anything that does not
// understand the layout can skip it safely.
//
// Block layout (each pair is (zoneId, confidence)):
//   byte 0      'S'
//   byte 1      'P'
//   byte 2      blockLen = bytes that follow (2 + 2*zoneCount), 2..18
//   byte 3      focusX  (0..255 on-screen centre of visual mass)
//   byte 4      focusY  (0..255)
//   bytes 5..   zoneId + confidence pairs (confidence 0..100)
//
// There is no pixel payload — the block is pure geometry, so even a dumb
// 1-watt receiver can place a wave without knowing the scene. Pure C++ / no
// dynamic memory so the host tests cover pack/parse/validation.
namespace sceneframe {

constexpr uint8_t  kSpatialMagic0        = 'S';
constexpr uint8_t  kSpatialMagic1        = 'P';
constexpr uint8_t  kSpatialMaxEntries    = 8;
// 3-byte header + 2 focus bytes + 2*8 zone bytes
constexpr uint8_t  kSpatialMaxBlock      = 3 + 2 + 2 * kSpatialMaxEntries;
// reserved Frame.flags bit signalling the block's presence
constexpr uint16_t SFLAG_SPATIAL_BLOCK   = (uint16_t)(1u << 13);

// Screen regions a companion can attribute visual mass to. Salience pairs let
// a receiver steer waves without any per-zone geometry of its own.
enum ZoneId : uint8_t {
  ZONE_LEFT = 0,
  ZONE_CENTER,
  ZONE_RIGHT,
  ZONE_TOP,
  ZONE_BOTTOM,
  ZONE_TOP_LEFT,
  ZONE_TOP_RIGHT,
  ZONE_BOTTOM_LEFT,
  ZONE_BOTTOM_RIGHT,
  ZONE_FULLSCREEN,
  ZONE_COUNT,
};

struct SpatialInfo {
  uint8_t focusX = 128;   // screen centre when unknown
  uint8_t focusY = 128;
  uint8_t zoneCount = 0;  // entries in `zones`, <= kSpatialMaxEntries
  struct Zone {
    uint8_t zone = ZONE_FULLSCREEN;
    uint8_t confidence = 0;  // 0..100
  } zones[kSpatialMaxEntries];
};

// Never reads past `avail` (the bytes that followed the base Frame). Returns
// false for malformed input (bad magic, bad/odd/oversized length, unknown
// zone id) so the receiver keeps its last good spatial sample.
inline bool parseSpatial(const uint8_t* p, size_t avail, SpatialInfo& out) {
  if (!p || avail < 3) return false;
  if (p[0] != kSpatialMagic0 || p[1] != kSpatialMagic1) return false;
  const size_t len = p[2];
  if (len < 2 || len > 2 + 2 * kSpatialMaxEntries) return false;
  if ((len & 1u) != 0) return false;
  if (avail - 3 < len) return false;
  const size_t n = (len - 2) / 2;
  if (n > kSpatialMaxEntries) return false;

  out.focusX = p[3];
  out.focusY = p[4];
  out.zoneCount = (uint8_t)n;
  for (size_t i = 0; i < n; ++i) {
    const uint8_t z = p[5 + 2 * i];
    if (z >= ZONE_COUNT) return false;  // unknown zone = corrupt block
    out.zones[i].zone = z;
    out.zones[i].confidence = p[6 + 2 * i];
  }
  return true;
}

// Fills dst (capacity `cap`) and returns the total block length in bytes, or
// 0 when the buffer is too small. Caller must set Frame.flags |= 
// SFLAG_SPATIAL_BLOCK when it returns non-zero.
inline size_t packSpatial(uint8_t* dst, size_t cap, const SpatialInfo& si) {
  const size_t n = si.zoneCount > kSpatialMaxEntries ? kSpatialMaxEntries
                                                     : si.zoneCount;
  const size_t len = 2 + 2 * n;
  const size_t total = 3 + len;
  if (!dst || cap < total) return 0;
  dst[0] = kSpatialMagic0;
  dst[1] = kSpatialMagic1;
  dst[2] = (uint8_t)len;
  dst[3] = si.focusX;
  dst[4] = si.focusY;
  for (size_t i = 0; i < n; ++i) {
    dst[5 + 2 * i] = si.zones[i].zone;
    dst[6 + 2 * i] = si.zones[i].confidence;
  }
  return total;
}

inline const char* zoneIdent(ZoneId z) {
  switch (z) {
    case ZONE_LEFT: return "left";
    case ZONE_CENTER: return "center";
    case ZONE_RIGHT: return "right";
    case ZONE_TOP: return "top";
    case ZONE_BOTTOM: return "bottom";
    case ZONE_TOP_LEFT: return "topleft";
    case ZONE_TOP_RIGHT: return "topright";
    case ZONE_BOTTOM_LEFT: return "bottomleft";
    case ZONE_BOTTOM_RIGHT: return "bottomright";
    case ZONE_FULLSCREEN: return "screen";
    default: return "none";
  }
}

inline const char* zoneLabel(ZoneId z) {
  switch (z) {
    case ZONE_LEFT: return "Left";
    case ZONE_CENTER: return "Centre";
    case ZONE_RIGHT: return "Right";
    case ZONE_TOP: return "Top";
    case ZONE_BOTTOM: return "Bottom";
    case ZONE_TOP_LEFT: return "Top-left";
    case ZONE_TOP_RIGHT: return "Top-right";
    case ZONE_BOTTOM_LEFT: return "Bottom-left";
    case ZONE_BOTTOM_RIGHT: return "Bottom-right";
    case ZONE_FULLSCREEN: return "Screen";
    default: return "Unknown";
  }
}

}  // namespace sceneframe