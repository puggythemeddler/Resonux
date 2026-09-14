import { describe, it, expect } from "vitest";
import os from "node:os";
import path from "node:path";
import fs from "node:fs";
import { AppCore } from "../core/app";
import { HttpError } from "../client/http";
import { runHardwareCheck, blipTarget } from "./check";
import type { CheckCalls } from "./check";
import type { StatusSnapshot, FrameSample, StatePayload } from "../domain/types";

async function waitFor(fn: () => boolean, timeoutMs = 5000, stepMs = 30): Promise<void> {
  const start = Date.now();
  while (!fn()) {
    if (Date.now() - start > timeoutMs) throw new Error("condition not met in time");
    await new Promise((r) => setTimeout(r, stepMs));
  }
}

const statusOk: StatusSnapshot = {
  ok: true,
  device: "Test Room",
  uptimeMs: 3 * 60_000,
  heap: 64 * 1024,
  fps: 30,
  stripCount: 1,
  wifi: { mode: "sta", ip: "192.168.1.20", connected: true },
  sync: { enabled: false, role: "off" },
  artnet: { enabled: false, fixtures: 0, status: "off" },
  system: { state: "running", action: "none" },
  display: { enabled: false, backlightPct: 100, timeoutS: 30, awake: true, wakePin: 0 },
};

const frameLive: FrameSample = {
  ok: true,
  fps: 30,
  amp: 0.5,
  bass: 0.4,
  lowMid: 0.3,
  mid: 0.3,
  highMid: 0.2,
  treble: 0.1,
  beat: false,
  beatStrength: 0,
  bandCount: 6,
  bands: [0.2, 0.3, 0.4, 0.3, 0.2, 0.1],
  peaks: [0.3, 0.4, 0.5, 0.4, 0.3, 0.2],
};

describe("blipTarget", () => {
  it("never picks 0 and never blasts full brightness", () => {
    expect(blipTarget(0)).toBe(40);
    expect(blipTarget(-5)).toBe(40);
    expect(blipTarget(255)).toBe(140);
    expect(blipTarget(200)).toBe(140);
    expect(blipTarget(100)).toBe(140);
    expect(blipTarget(10)).toBe(50);
  });
});

describe("runHardwareCheck (pure, stubbed)", () => {
  const target = { id: "t1", name: "Test Room", isSimulator: false };
  const fastOpts = { audioSpacingMs: 0, blipHoldMs: 0 };

  it("passes every step on a healthy, live controller and restores brightness", async () => {
    let brightness = 60;
    let sample = 0;
    const liveAmps = [0.1, 0.55, 0.2, 0.7, 0.35, 0.9];
    const calls: CheckCalls = {
      fetchStatus: async () => statusOk,
      fetchFrame: async () => ({ ...frameLive, amp: liveAmps[sample++ % liveAmps.length] }),
      fetchState: async () => ({ theme: { global: "g", strips: [] }, effect: [1], brightness, stripCount: 1, themeCount: 1 }),
      setBrightness: async (v) => {
        brightness = v;
        return true;
      },
    };

    const rep = await runHardwareCheck(target, calls, fastOpts);
    expect(rep.summary.fail).toBe(0);
    expect(rep.summary.warn).toBe(0);
    expect(rep.steps.every((s) => s.verdict === "pass")).toBe(true);
    expect(rep.visual).toEqual({ from: 60, to: 100 });
    // Brightness came home.
    expect(brightness).toBe(60);
  });

  it("fails fast when the controller does not answer", async () => {
    const calls: CheckCalls = {
      fetchStatus: async () => {
        throw new HttpError(0, "timeout", "no reply");
      },
      fetchFrame: async () => frameLive,
      fetchState: async () => ({ theme: { global: "g", strips: [] }, effect: [1], brightness: 60, stripCount: 1, themeCount: 1 }),
      setBrightness: async () => true,
    };

    const rep = await runHardwareCheck(target, calls, fastOpts);
    expect(rep.summary.fail).toBeGreaterThan(0);
    expect(rep.steps[0].verdict).toBe("fail");
    expect(rep.summary.pass).toBe(0);
  });

  it("warns (does not fail) when the audio signal is flat", async () => {
    let brightness = 60;
    const calls: CheckCalls = {
      fetchStatus: async () => statusOk,
      fetchFrame: async () => ({ ...frameLive, amp: 0.2 }),
      fetchState: async () => ({ theme: { global: "g", strips: [] }, effect: [1], brightness, stripCount: 1, themeCount: 1 }),
      setBrightness: async (v) => {
        brightness = v;
        return true;
      },
    };

    const rep = await runHardwareCheck(target, calls, fastOpts);
    const audio = rep.steps.find((s) => s.id === "audio")!;
    expect(audio.verdict).toBe("warn");
    expect(rep.summary.fail).toBe(0);
  });

  it("warns (does not fail) when the write round-trip disagrees", async () => {
    let brightness = 60;
    const calls: CheckCalls = {
      fetchStatus: async () => statusOk,
      fetchFrame: async () => frameLive,
      fetchState: async () => ({ theme: { global: "g", strips: [] }, effect: [1], brightness, stripCount: 1, themeCount: 1 }),
      setBrightness: async (v) => {
        // Controller accepts the blip but refuses to go back home.
        if (v === 60) return false;
        brightness = v;
        return true;
      },
    };

    const rep = await runHardwareCheck(target, calls, fastOpts);
    const write = rep.steps.find((s) => s.id === "write")!;
    expect(write.verdict).toBe("warn");
    expect(rep.summary.fail).toBe(0);
  });
});

describe("AppCore runHardwareCheck (against the in-process simulator)", () => {
  it("reports every step passing and identifies the simulator honestly", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-check-"));
    const core = new AppCore({ dataDir: dir, appInfo: { version: "0.0.0-test" } });
    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;

      // Give the check real pacing except the human-visible blip hold.
      const rep = await core.runHardwareCheck(id);
      expect(rep.targetId).toBe(id);
      expect(rep.isSimulator).toBe(true);
      expect(rep.summary.fail).toBe(0);
      expect(rep.visual).not.toBeNull();
      // Brightness fully restored.
      const st = await core.getState(id);
      expect(st.brightness).toBe(255); // mock default
      expect(rep.note).toContain("simulator");

      await expect(core.runHardwareCheck("nope")).rejects.toThrow(HttpError);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });
});