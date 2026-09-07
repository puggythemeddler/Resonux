#pragma once
#include <cstdint>

constexpr uint16_t kArtNetPort        = 6454;
constexpr uint16_t kArtNetHeaderSize  = 12;
constexpr uint16_t kArtDmxSize        = 18;   // header + dmx data start
constexpr uint16_t kArtPollReplySize  = 239;
constexpr uint8_t  kArtNetVersionHi   = 0;
constexpr uint8_t  kArtNetVersionLo   = 14;
constexpr uint8_t  kArtNetProtoHi     = 0;
constexpr uint8_t  kArtNetProtoLo     = 14;

enum ArtNetOpCode : uint16_t {
  ARTNET_OP_POLL      = 0x2000,
  ARTNET_OP_POLL_REPLY = 0x2100,
  ARTNET_OP_DMX       = 0x5000,
  ARTNET_OP_SYNC      = 0x8800,
};

#pragma pack(push, 1)
struct ArtNetHeader {
  char     id[8];
  uint16_t opcode;
  uint16_t versionHi;
  uint8_t  flags;
  uint8_t  diagPriority;
  uint16_t spare;
  uint8_t  oemHi;
  uint8_t  oemLo;
  uint8_t  ubeaVersion;
  uint16_t status1;
  uint16_t etsaCode;
  char     shortName[18];
  char     longName[64];
  char     report[64];
};

struct ArtNetPoll {
  char     id[8];
  uint16_t opcode;
  uint16_t versionHi;
  uint8_t  flags;
  uint8_t  diagPriority;
};

struct ArtNetPollReply {
  char     id[8];
  uint16_t opcode;
  uint8_t  ip[4];
  uint16_t port;
  uint8_t  versionHi;
  uint8_t  versionLo;
  uint8_t  net;
  uint8_t  sub;
  uint8_t  oemHi;
  uint8_t  oemLo;
  uint8_t  ubeaVersion;
  uint8_t  status1;
  uint16_t estaCode;
  char     shortName[18];
  char     longName[64];
  char     report[64];
  uint8_t  numPortsHi;
  uint8_t  numPortsLo;
  uint8_t  portTypes[4];
  uint8_t  goodInput[4];
  uint8_t  goodOutput[4];
  uint8_t  swIn[4];
  uint8_t  swOut[4];
  uint8_t  swVideo;
  uint8_t  swMacro;
  uint8_t  swRemote;
  uint8_t  spare[3];
  uint8_t  style;
  uint8_t  mac[6];
  uint8_t  bindIp[4];
  uint8_t  bindIndex;
  uint8_t  status2;
  uint8_t  filler[26];
};

struct ArtNetDmx {
  char     id[8];
  uint16_t opcode;
  uint16_t versionHi;
  uint8_t  sequence;
  uint8_t  physical;
  uint16_t subUni;
  uint8_t  lengthHi;
  uint8_t  lengthLo;
  uint8_t  data[512];
};
#pragma pack(pop)

inline void artnetFillHeader(char* buf) {
  const char kHeader[] = "Art-Net\0";
  for (int i = 0; i < 8; ++i) buf[i] = kHeader[i];
}
