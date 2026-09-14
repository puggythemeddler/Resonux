import { describe, it, expect } from "vitest";
import {
  buildDiscoverResponse,
  parseDiscoverResponse,
  kDiscoverRespPrefix,
} from "./discovery";

describe("RESO_DISCOVER codec", () => {
  it("round-trips a firmware-shaped reply", () => {
    const line = buildDiscoverResponse({
      id: "a1b2c3",
      name: "Living Room",
      fw: "0.7.0",
      role: "master",
      caps: 42,
      source: 4,
    });
    expect(line.startsWith(kDiscoverRespPrefix)).toBe(true);

    const e = parseDiscoverResponse(line);
    expect(e.valid).toBe(true);
    expect(e.id).toBe("a1b2c3");
    // Firmware space-sanitizes names; parser desalinates nothing (documents it).
    expect(e.name).toBe("Living_Room");
    expect(e.caps).toBe(42);
    expect(e.source).toBe(4);
    expect(e.fw).toBe("0.7.0");
    expect(e.role).toBe("master");
    expect(e.kindId).toBe("network");
  });

  it("parses a raw firmware-style response with kind field", () => {
    const e = parseDiscoverResponse(
      "RESO-DISCOVER-RESP id=abc name=Bedroom kind=bluetooth caps=3 source=1 fw=0.4.1 role=slave"
    );
    expect(e.valid).toBe(true);
    expect(e.id).toBe("abc");
    expect(e.name).toBe("Bedroom");
    expect(e.kindId).toBe("bluetooth");
    expect(e.caps).toBe(3);
    expect(e.source).toBe(1);
    expect(e.fw).toBe("0.4.1");
    expect(e.role).toBe("slave");
  });

  it("ignores unknown keys and keeps parsing the rest", () => {
    const e = parseDiscoverResponse(
      "RESO-DISCOVER-RESP weather=sunny id=found4 name=Studio fw=0.9.0"
    );
    expect(e.valid).toBe(true);
    expect(e.id).toBe("found4");
    expect(e.fw).toBe("0.9.0");
  });

  it("rejects a non-responder datagram", () => {
    const e = parseDiscoverResponse("RESO_DISCOVER v1\n");
    expect(e.valid).toBe(false);
    expect(e.id).toBe("");
  });

  it("handles space-heavy values without corruption", () => {
    const e = parseDiscoverResponse(
      "RESO-DISCOVER-RESP id=x name=a_b_c kind=network caps=0 source=0 fw=0.1.0 role="
    );
    expect(e.valid).toBe(true);
    expect(e.name).toBe("a_b_c");
    expect(e.role).toBe("");
  });
});