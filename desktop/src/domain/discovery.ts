// TypeScript port of firmware/src/device/DiscoverProtocol.h — the pure
// RESO_DISCOVER wire codec. Kept byte-compatible with the ESP32 responder so
// any controller firmware answers a scanner built from this module, and vice
// versa. Wire format is a single UDP datagram:
//
//   probe:  "RESO_DISCOVER v1\n"
//   reply:  "RESO-DISCOVER-RESP id=<id> name=<name> kind=<conn> caps=<n>
//             source=<n> fw=<v> role=<r>"      (key=value, space separated)
//
// The parser is deliberately lenient: unknown keys are ignored so an older
// responder can be scanned by a newer scanner (and vice versa).

export const kDiscoverPort = 9770;
export const kDiscoverGroup = "239.255.42.10";
export const kDiscoverProbe = "RESO_DISCOVER v1\n";
export const kDiscoverRespPrefix = "RESO-DISCOVER-RESP";
export const kDiscoverProbeIntervalMs = 1500;

export const MAX_ID_LEN = 32;
export const MAX_NAME_LEN = 32;
export const MAX_FW_LEN = 16;
export const MAX_ROLE_LEN = 16;

export interface DiscoverEnvelope {
  id: string;
  name: string;
  fw: string;
  role: string;
  kindId: string;
  caps: number;
  source: number;
  valid: boolean;
}

function tokenValue(line: string, keyLen: number): string {
  // line starts at a key: value; keyLen excludes '='.
  const eq = line.indexOf("=");
  if (eq !== keyLen) return "";
  return line.slice(eq + 1);
}

export function parseDiscoverResponse(buf: string | Buffer): DiscoverEnvelope {
  const e: DiscoverEnvelope = {
    id: "",
    name: "",
    fw: "",
    role: "",
    kindId: "",
    caps: 0,
    source: 0,
    valid: false,
  };
  const text = typeof buf === "string" ? buf : buf.toString("utf8");
  if (text.length < kDiscoverRespPrefix.length) return e;
  if (!text.startsWith(kDiscoverRespPrefix)) return e;

  let pos = kDiscoverRespPrefix.length;
  let sawId = false;
  while (pos < text.length) {
    while (pos < text.length && text[pos] === " ") pos++;
    if (pos >= text.length) break;
    const nl = text.indexOf(" ", pos);
    const field = nl === -1 ? text.slice(pos) : text.slice(pos, nl);
    let value = "";
    if (field.startsWith("id=") && field.length > 3) {
      value = tokenValue(field, 2);
      if (value.length > MAX_ID_LEN) value = value.slice(0, MAX_ID_LEN);
      e.id = value;
      sawId = true;
    } else if (field.startsWith("name=") && field.length > 5) {
      value = tokenValue(field, 4);
      if (value.length > MAX_NAME_LEN) value = value.slice(0, MAX_NAME_LEN);
      e.name = value;
    } else if (field.startsWith("kind=") && field.length > 5) {
      value = tokenValue(field, 4);
      if (value.length > 24) value = value.slice(0, 24);
      e.kindId = value;
    } else if (field.startsWith("caps=") && field.length > 5) {
      value = tokenValue(field, 4);
      const n = parseInt(value, 10);
      if (!Number.isNaN(n)) e.caps = n >>> 0;
    } else if (field.startsWith("source=") && field.length > 7) {
      value = tokenValue(field, 6);
      const n = parseInt(value, 10);
      e.source = Number.isNaN(n) ? 0 : n;
    } else if (field.startsWith("fw=") && field.length > 3) {
      value = tokenValue(field, 2);
      if (value.length > MAX_FW_LEN) value = value.slice(0, MAX_FW_LEN);
      e.fw = value;
    } else if (field.startsWith("role=") && field.length > 5) {
      value = tokenValue(field, 4);
      if (value.length > MAX_ROLE_LEN) value = value.slice(0, MAX_ROLE_LEN);
      e.role = value;
    }
    pos = nl === -1 ? text.length : nl + 1;
  }
  e.valid = sawId;
  return e;
}

// Builds a reply line mirroring firmware buildDiscoverResponse (spaces are
// sanitized so the token stream round-trips). Used by the simulator's virtual
// responder and by tests.
export function buildDiscoverResponse(opts: {
  id: string;
  name?: string;
  kindIdent?: string;
  caps?: number;
  source?: number;
  fw?: string;
  role?: string;
}): string {
  const sanitize = (s: string) => s.replace(/ /g, "_");
  const parts = [
    kDiscoverRespPrefix,
    `id=${sanitize(opts.id)}`,
    `name=${sanitize(opts.name ?? opts.id)}`,
    `kind=${sanitize(opts.kindIdent ?? "network")}`,
    `caps=${(opts.caps ?? 0).toString()}`,
    `source=${(opts.source ?? 0).toString()}`,
    `fw=${sanitize(opts.fw ?? "")}`,
    `role=${sanitize(opts.role ?? "")}`,
  ];
  // Trim trailing space like the firmware does.
  return parts.join(" ").replace(/ +$/, "");
}