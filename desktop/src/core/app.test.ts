import { describe, it, expect } from "vitest";
import os from "node:os";
import path from "node:path";
import fs from "node:fs";
import { AppCore } from "./app";

async function waitFor(fn: () => boolean, timeoutMs = 5000, stepMs = 30): Promise<void> {
  const start = Date.now();
  while (!fn()) {
    if (Date.now() - start > timeoutMs) throw new Error("condition not met in time");
    await new Promise((r) => setTimeout(r, stepMs));
  }
}

describe("AppCore", () => {
  it("boots, self-starts the simulator, and serves a live snapshot", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-appcore-"));
    const core = new AppCore({
      dataDir: dir,
      appInfo: { version: "0.0.0-test" },
    });

    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));

      const snap = core.snapshot();
      expect(snap.simulatorRunning).toBe(true);
      const sim = snap.controllers.find((c) => c.kind === "simulator")!;
      expect(sim.health).toBe("ok");
      expect(sim.address).toBe("127.0.0.1");
      // Not selected yet → honest null selection.
      expect(snap.selected).toBeNull();

      core.selectController(sim.id);
      const after = core.snapshot();
      expect(after.selectedId).toBe(sim.id);
      expect(after.selected?.status.ok).toBe(true);

      // Commands round-trip through the core into the running simulator.
      const ok = await core.setBrightness(sim.id, 128);
      expect(ok).toBe(true);
      const cinematic = await core.getCinematic(sim.id);
      expect(cinematic.active).toBe(true);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("stops the simulator on request and drops it from the snapshot", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-appcore-"));
    const core = new AppCore({
      dataDir: dir,
      appInfo: { version: "0.0.0-test" },
    });

    try {
      await waitFor(() => core.snapshot().controllers.length > 0);
      core.setSimulator(false);
      await waitFor(() => core.snapshot().controllers.length === 0);
      expect(core.snapshot().simulatorRunning).toBe(false);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });
});