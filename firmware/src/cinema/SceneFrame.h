#pragma once
#include <stdint.h>
#include <string.h>

// SceneFrame wire codec for the companion scene analyzer (Cinematic Mode,
// spec §24-§30). The companion app watches a movie/TV source and emits a small
// binary frame describing what is on screen (scene kind, events, average
// luminance, dominant colour, motion, observed program audio). The ESP32
// receiver decodes it and fuses it with its own on-board audio analysis.
//
// Pure C++ (no Arduino / no FreeRTOS) so the host-side unit tests cover
// pack/unpack/validation. The UDP transport itself lives in SceneLinkNode.

namespace sceneframe {

constexpr uint32_t kSceneMagic       = 0x53434E52u;  // 'RNCS' little-endian
constexpr uint16_t kSceneVersion     = 1;
constexpr uint16_t kSceneDefaultPort = 9772;
constexpr char     kSceneDefaultGroup[] = "239.255.42.11";

// ---- scene kinds (companion video analysis) --------------------------------
enum SceneKind : uint8_t {
  SCENE_UNDEFINED = 0,
  SCENE_SPEECH,      // dialogue / talking heads
  SCENE_QUIET,       // near-black / near-silence, subtle ambience
  SCENE_ACTION,      // broad activity, fast cuts
  SCENE_CHASE,       // fast motion, cut-heavy
  SCENE_EXPLOSION,   // one big moment (single event, not sustained)
  SCENE_MUSIC,       // music-led sequence / montage
};

// ---- discrete events (transient, decayed by the engine) --------------------
enum SceneEvent : uint8_t {
  SEVENT_NONE = 0,
  SEVENT_WHISPER,    // hushed, dim, cool
  SEVENT_FLASH,      // bright flash (gunshot / lightning / quick cut)
  SEVENT_BOOM,       // dramatic impact (explosion / crash)
  SEVENT_DARK,       // sudden darkness / drop
  SEVENT_CHANGE,     // hard scene cut
};

// ---- companion-side flags --------------------------------------------------
enum SceneSourceFlag : uint8_t {
  SFLAG_PROGRAM_AUDIO = 1u << 0,  // companion monitors the program audio track
  SFLAG_PILLARBOX     = 1u << 1,  // letter/pillarboxed (not edge-to-edge)
};

// Frame.flags lives in the 16-bit `flags` field. Bit 13 is reserved for the
// spatial extension (a length-tagged block appended after this struct, see
// SpatialBlock.h); it is kept out of SceneSourceFlag because that field is
// only a byte.

#pragma pack(push, 1)
struct Frame {
  uint32_t magic = 0;
  uint16_t version = 0;
  uint16_t flags = 0;            // reserved for future use
  uint32_t seq = 0;              // companion frame counter
  uint32_t hostTimeMs = 0;       // companion clock (probe for rate/staleness)
  uint8_t  sceneId = SCENE_UNDEFINED;
  uint8_t  eventId = SEVENT_NONE;
  uint8_t  eventConfidence = 0;  // 0..100
  uint8_t  avgLuminance = 0;     // 0..255 scene average
  uint8_t  hue = 0;              // dominant colour hue  (0..255 -> 0..360°)
  uint8_t  sat = 0;              // dominant colour saturation 0..255
  uint8_t  val = 0;              // dominant colour value 0..255
  uint8_t  motion = 0;           // 0..255 global motion energy
  uint8_t  progAudio = 0;        // 0..255 companion-observed program audio
  uint8_t  sourceFlags = 0;      // SceneSourceFlag bits
  uint8_t  pad[2] = {0, 0};
};
#pragma pack(pop)

inline uint16_t frameBytes() { return sizeof(Frame); }

// bytes == number of bytes actually received off the wire. Over-long payloads
// are accepted (caller may trim); short or corrupt payloads are rejected.
inline bool validFrame(const Frame& f, size_t bytes) {
  if (bytes < sizeof(Frame)) return false;
  if (f.magic != kSceneMagic || f.version != kSceneVersion) return false;
  if (f.sceneId > SCENE_MUSIC) return false;
  if (f.eventId > SEVENT_CHANGE) return false;
  return true;
}

inline void packFrame(Frame& f, uint32_t seq, uint32_t hostMs, SceneKind scene,
                      SceneEvent ev, uint8_t conf, uint8_t lum, uint8_t h,
                      uint8_t s, uint8_t v, uint8_t motion, uint8_t progAudio,
                      uint8_t sourceFlags, uint16_t frameFlags = 0) {
  f.magic = kSceneMagic;
  f.version = kSceneVersion;
  f.flags = frameFlags;
  f.seq = seq;
  f.hostTimeMs = hostMs;
  f.sceneId = (uint8_t)scene;
  f.eventId = (uint8_t)ev;
  f.eventConfidence = conf > 100 ? (uint8_t)100 : conf;
  f.avgLuminance = lum;
  f.hue = h;
  f.sat = s;
  f.val = v;
  f.motion = motion;
  f.progAudio = progAudio;
  f.sourceFlags = sourceFlags;
  f.pad[0] = 0;
  f.pad[1] = 0;
}

inline const char* sceneIdent(SceneKind k) {
  switch (k) {
    case SCENE_SPEECH: return "speech";
    case SCENE_QUIET: return "quiet";
    case SCENE_ACTION: return "action";
    case SCENE_CHASE: return "chase";
    case SCENE_EXPLOSION: return "explosion";
    case SCENE_MUSIC: return "music";
    default: return "undefined";
  }
}

inline const char* sceneLabel(SceneKind k) {
  switch (k) {
    case SCENE_SPEECH: return "Speech";
    case SCENE_QUIET: return "Quiet";
    case SCENE_ACTION: return "Action";
    case SCENE_CHASE: return "Chase";
    case SCENE_EXPLOSION: return "Explosion";
    case SCENE_MUSIC: return "Music";
    default: return "Undefined";
  }
}

inline const char* eventIdent(SceneEvent e) {
  switch (e) {
    case SEVENT_WHISPER: return "whisper";
    case SEVENT_FLASH: return "flash";
    case SEVENT_BOOM: return "boom";
    case SEVENT_DARK: return "dark";
    case SEVENT_CHANGE: return "change";
    default: return "none";
  }
}

inline const char* eventLabel(SceneEvent e) {
  switch (e) {
    case SEVENT_WHISPER: return "Whisper";
    case SEVENT_FLASH: return "Flash";
    case SEVENT_BOOM: return "Boom";
    case SEVENT_DARK: return "Dark";
    case SEVENT_CHANGE: return "Scene cut";
    default: return "None";
  }
}

}  // namespace sceneframe