import { describe, it, expect } from "vitest";
import { buildReport, suggestedReportName } from "./report";
import type { AppInfo, ControllerInfo, StatusSnapshot, HardwareCheckReport } from "../domain/bridge";

const app: AppInfo = { version: "0.0.0-test", platform: "win32", isPackaged: false };

const controller: ControllerInfo = {
  id: "sim-demo",
  name: "Demo Room",
  kind: "simulator",
  address: "127.0.0.1",
  port: 12345,
  fw: "1.0",
  role: "discover",
  caps: 1,
  lastSeenMs: Date.now(),
  online: true,
  health: "ok",
  latencyMs: 2,
};

const status: StatusSnapshot = {
  ok: true,
  device: "Demo Room",
  uptimeMs: 1000,
  heap: 65536,
  fps: 30,
  stripCount: 2,
  wifi: { mode: "sta", ip: "192.168.1.44", connected: true },
  sync: { enabled: false, role: "off" },
  artnet: { enabled: false, fixtures: 0, status: "off" },
  system: { state: "running", action: "none" },
  display: { enabled: true, backlightPct: 100, timeoutS: 30, awake: true, wakePin: 5 },
};

const check: HardwareCheckReport = {
  targetId: "sim-demo",
  targetName: "Demo Room",
  isSimulator: true,
  steps: [
    { id: "reachability", label: "Controller answers", detail: "It replied instantly.", verdict: "pass" },
  ],
  summary: { pass: 1, warn: 0, fail: 0 },
  visual: { from: 60, to: 100 },
  note: "A computer can verify the controller and its connections, but only eyes can confirm the LEDs are glowing.",
};

describe("buildReport", () => {
  it("serialises the app, controller facts, check and session log as JSON", () => {
    const text = buildReport({ app, controller, status, check, log: [{ seq: 0, at: "2026-01-01T00:00:00Z", level: "info", kind: "check", message: "Hardware check finished" }] });
    const doc = JSON.parse(text) as {
      app: { version: string };
      controller: { name: string; kind: string };
      hardwareCheck: { isSimulator: boolean };
      sessionLog: { length: number };
    };
    expect(doc.app.version).toBe("0.0.0-test");
    expect(doc.controller.name).toBe("Demo Room");
    expect(doc.controller.kind).toBe("simulator");
    expect(doc.hardwareCheck.isSimulator).toBe(true);
    expect(doc.sessionLog.length).toBe(1);
  });

  it("never includes network addresses, Wi-Fi details, or credentials", () => {
    const text = buildReport({ app, controller, status, check, log: [{ seq: 0, at: "2026-01-01T00:00:00Z", level: "warn", kind: "config", message: "Wi-Fi network set" }] });
    expect(text).not.toContain("127.0.0.1");
    expect(text).not.toContain("192.168");
    expect(text).not.toContain("staPassword");
    expect(text).not.toContain("staSsid");
    expect(text).not.toContain("apPassword");

    const doc = JSON.parse(text) as { controller: Record<string, unknown> };
    expect(doc.controller.address).toBeUndefined();
    expect(doc.controller.ip).toBeUndefined();
    expect(doc.controller.port).toBeUndefined();
    expect(doc.controller.staPassword).toBeUndefined();
  });

  it("serialises a null check honestly (exported before any check ran)", () => {
    const text = buildReport({ app, controller, status, check: null, log: [] });
    const doc = JSON.parse(text) as { hardwareCheck: unknown };
    expect(doc.hardwareCheck).toBeNull();
  });
});

describe("suggestedReportName", () => {
  it("sanitises the controller name and defaults gracefully", () => {
    expect(suggestedReportName("Demo Room")).toMatch(/^resonux-report-Demo-Room-\d{4}-\d{2}-\d{2}\.json$/);
    expect(suggestedReportName("!!!")).toMatch(/^resonux-report-controller-/);
  });
});