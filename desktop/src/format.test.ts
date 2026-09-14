import { describe, it, expect } from "vitest";
import { fmtSystemState } from "../ui/format";

describe("fmtSystemState", () => {
  it("maps firmware state tokens to plain language", () => {
    expect(fmtSystemState("reactive")).toBe("Reactive");
    expect(fmtSystemState("sleeping")).toBe("Sleeping");
    expect(fmtSystemState("booting")).toBe("Starting up");
  });

  it("neutral-cases unknown states instead of leaking raw tokens", () => {
    expect(fmtSystemState("calibrating")).toBe("Calibrating");
    expect(fmtSystemState("")).toBe("—");
  });
});