#pragma once
#include "device/DeviceTypes.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Pure RESO_DISCOVER wire codec — no Arduino. Both the responder (DeviceManager)
// and any (future) host tools share one implementation, and the parser is
// host-testable. The wire format is a single UDP datagram:
//
//   probe:  "RESO_DISCOVER v1\n"
//   reply:  "RESO-DISCOVER-RESP id=<id> name=<name> kind=<conn> caps=<n>
//             source=<n> fw=<v> role=<r>"      (key=value, space separated)
//
// The parser is deliberately lenient: unknown keys are ignored so an older
// responder can be scanned by a newer scanner (and vice versa). Value fields
// with spaces are replaced by '_' on encode.

namespace dev {

inline constexpr uint16_t kDiscoverPort = 9770;
inline constexpr char kDiscoverGroup[] = "239.255.42.10";
inline constexpr char kDiscoverProbe[] = "RESO_DISCOVER v1\n";
inline constexpr char kDiscoverRespPrefix[] = "RESO-DISCOVER-RESP";

// 'kind' on the wire is a ConnectionType ident (e.g. "network", "bluetooth").
inline ConnectionType connectionFromIdent(const char* id) {
  if (!id) return CONN_UNKNOWN;
  for (int i = 0; i < kConnectionCount; ++i) {
    if (strcmp(connectionAt(i).id, id) == 0) return connectionAt(i).kind;
  }
  return CONN_UNKNOWN;
}

struct DiscoverEnvelope {
  char            id[kMaxDeviceIdLen] = "";
  char            name[kMaxDeviceNameLen] = "";
  char            fw[kMaxFwLen] = "";
  char            role[kMaxRoleLen] = "";
  ConnectionType  conn = CONN_UNKNOWN;
  uint32_t        caps = CAP_NONE;
  DiscoverySource source = SRC_UNKNOWN;
  bool            valid = false;
};

inline DiscoverEnvelope parseDiscoverResponse(const char* buf, int len) {
  DiscoverEnvelope e;
  if (!buf || len <= 0) return e;
  const size_t prefixLen = sizeof(kDiscoverRespPrefix) - 1;
  if ((size_t)len < prefixLen) return e;
  if (strncmp(buf, kDiscoverRespPrefix, prefixLen) != 0) return e;

  const char* p = buf + prefixLen;
  bool sawId = false;
  while (p && *p) {
    while (*p == ' ') ++p;
    if (!*p) break;
    const char* nl = strchr(p, ' ');
    size_t tl = nl ? (size_t)(nl - p) : strlen(p);
    const char* eq = strchr(p, '=');
    if (!eq) break;
    size_t kl = (size_t)(eq - p);
    size_t vl = tl - kl - 1;

    if (kl == 2 && strncmp(p, "id=", 3) == 0 && vl > 0) {
      size_t n = vl < sizeof(e.id) ? vl : sizeof(e.id) - 1;
      memcpy(e.id, eq + 1, n);
      e.id[n] = '\0';
      sawId = true;
    } else if (kl == 4 && strncmp(p, "name=", 5) == 0 && vl > 0) {
      size_t n = vl < sizeof(e.name) ? vl : sizeof(e.name) - 1;
      memcpy(e.name, eq + 1, n);
      e.name[n] = '\0';
    } else if (kl == 4 && strncmp(p, "kind=", 5) == 0 && vl > 0) {
      char kind[24] = "";
      size_t n = vl < sizeof(kind) ? vl : sizeof(kind) - 1;
      memcpy(kind, eq + 1, n);
      kind[n] = '\0';
      e.conn = connectionFromIdent(kind);
      if (e.conn == CONN_UNKNOWN) e.conn = CONN_NETWORK;
    } else if (kl == 4 && strncmp(p, "caps=", 5) == 0 && vl > 0) {
      char tmp[16] = "";
      size_t n = vl < sizeof(tmp) - 1 ? vl : sizeof(tmp) - 1;
      memcpy(tmp, eq + 1, n);
      tmp[n] = '\0';
      e.caps = (uint32_t)strtoul(tmp, nullptr, 10);
    } else if (kl == 6 && strncmp(p, "source=", 7) == 0 && vl > 0) {
      char tmp[8] = "";
      size_t n = vl < sizeof(tmp) - 1 ? vl : sizeof(tmp) - 1;
      memcpy(tmp, eq + 1, n);
      tmp[n] = '\0';
      int s = atoi(tmp);
      e.source = (s > 0 && s < SRC_COUNT) ? (DiscoverySource)s : SRC_NETWORK;
    } else if (kl == 2 && strncmp(p, "fw=", 3) == 0 && vl > 0) {
      size_t n = vl < sizeof(e.fw) ? vl : sizeof(e.fw) - 1;
      memcpy(e.fw, eq + 1, n);
      e.fw[n] = '\0';
    } else if (kl == 4 && strncmp(p, "role=", 5) == 0 && vl > 0) {
      size_t n = vl < sizeof(e.role) ? vl : sizeof(e.role) - 1;
      memcpy(e.role, eq + 1, n);
      e.role[n] = '\0';
    }
    p = nl;
  }
  e.valid = sawId;
  return e;
}

// Encodes a reply line into `out` (bounded by cap). Fields with spaces are
// space-sanitized so the token stream round-trips through parseDiscoverResponse.
inline void writeField(char*& o, size_t& left, const char* key,
                       const char* value) {
  if (!o || !key) return;
  const size_t kl = strlen(key);
  size_t vl = 0;
  if (value) {
    for (const char* c = value; *c && vl < 96; ++c) vl++;
  }
  // Need room for key, '=', value (<= vl, rounded), ' ' and a trailing NUL.
  if (left <= kl + vl + 3) return;
  memcpy(o, key, kl);
  o += kl;
  *o++ = '=';
  if (value) {
    for (const char* c = value; *c; ++c) {
      *o++ = (*c == ' ') ? '_' : *c;
    }
  }
  *o++ = ' ';
  *o = '\0';
  left -= kl + vl + 2;
}

inline void buildDiscoverResponse(char* out, size_t cap, const char* id,
                                  const char* name, const char* kindIdent,
                                  uint32_t caps, DiscoverySource source,
                                  const char* fw, const char* role) {
  if (!out || cap == 0) return;
  out[0] = '\0';
  char* o = out;
  size_t left = cap;
  const size_t pfx = sizeof(kDiscoverRespPrefix) - 1;
  if (left > pfx + 1) {
    memcpy(o, kDiscoverRespPrefix, pfx);
    o += pfx;
    *o++ = ' ';
    *o = '\0';
    left -= pfx + 1;
  }
  char num[16];
  writeField(o, left, "id", id ? id : "");
  writeField(o, left, "name", name ? name : id ? id : "");
  writeField(o, left, "kind", kindIdent ? kindIdent : "network");
  snprintf(num, sizeof(num), "%lu", (unsigned long)caps);
  writeField(o, left, "caps", num);
  snprintf(num, sizeof(num), "%d", (int)source);
  writeField(o, left, "source", num);
  writeField(o, left, "fw", fw ? fw : "");
  writeField(o, left, "role", role ? role : "");
  // Trim any trailing space.
  size_t len = strlen(out);
  while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
}

}  // namespace dev