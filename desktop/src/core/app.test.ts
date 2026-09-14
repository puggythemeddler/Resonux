import { describe, it, expect } from "vitest";
import os from "node:os";
import path from "node:path";
import fs from "node:fs";
import { AppCore } from "./app";
import { get } from "../client/http";
import type { ConfigPayload, ThemesPayload, StatePayload, AudioSourcesPayload } from "../domain/types";

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

  it("renames and sets Wi-Fi with a byte-exact backup left behind", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-appcore-"));
    const core = new AppCore({
      dataDir: dir,
      appInfo: { version: "0.0.0-test" },
    });

    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;

      const empty = await core.renameController(id, "   ");
      expect(empty.ok).toBe(false);

      const r = await core.renameController(id, "Studio");
      expect(r.ok).toBe(true);
      expect(r.rebootApplied).toBe(true);
      expect(r.backupPath).toBeTruthy();

      const backupsDir = path.join(dir, "backups");
      const files = fs.readdirSync(backupsDir).filter((f) => f.startsWith("config-"));
      expect(files.length).toBeGreaterThan(0);
      expect(fs.readFileSync(path.join(backupsDir, files[0]), "utf8")).toContain("deviceName");

      expect((await core.getStatus(id)).device).toBe("Studio");

      const w = await core.setWifi(id, "HomeNet", "not-logged-here");
      expect(w.ok).toBe(true);
      expect(w.wifiApplied).toBe(true);

      const sim = core.snapshot().controllers.find((c) => c.kind === "simulator")!;
      const cfg = await get<ConfigPayload>({ address: sim.address, port: sim.port }, "/api/config");
      expect(cfg.net!.staSsid).toBe("HomeNet");
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("nags on first run until a controller is chosen or the wizard is finished", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-appcore-"));
    const core = new AppCore({
      dataDir: dir,
      appInfo: { version: "0.0.0-test" },
    });

    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      expect(core.snapshot().wizardNeeded).toBe(true);
      expect(core.snapshot().settings.wizardCompleted).toBe(false);

      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;
      core.selectController(id);
      expect(core.snapshot().wizardNeeded).toBe(false);
      core.selectController(null);
      expect(core.snapshot().wizardNeeded).toBe(true);

      core.finishWizard();
      expect(core.snapshot().wizardNeeded).toBe(false);
      expect(core.snapshot().settings.wizardCompleted).toBe(true);

      const saved = JSON.parse(fs.readFileSync(path.join(dir, "settings.json"), "utf8")) as {
        wizardCompleted?: boolean;
      };
      expect(saved.wizardCompleted).toBe(true);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("waitOnline answers true while the (restart-less) simulator is up", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-appcore-"));
    const core = new AppCore({
      dataDir: dir,
      appInfo: { version: "0.0.0-test" },
    });

    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;
      expect(await core.waitOnline(id, 3000)).toBe(true);
      expect(await core.waitOnline("nope", 200)).toBe(false);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });
});

describe("AppCore D3 control surface", () => {
  it("getState returns theme/effect/brightness payload", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-d3-state-"));
    const core = new AppCore({ dataDir: dir, appInfo: { version: "0.0.0-test" } });
    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;
      const st = await core.getState(id);
      expect(st.stripCount).toBeGreaterThanOrEqual(1);
      expect(typeof st.brightness).toBe("number");
      expect(Array.isArray(st.effect)).toBe(true);
      expect(Array.isArray(st.theme.strips)).toBe(true);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("selectTheme changes the global and per-strip themes", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-d3-thsel-"));
    const core = new AppCore({ dataDir: dir, appInfo: { version: "0.0.0-test" } });
    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;

      expect(await core.selectTheme(id, "ocean")).toBe(true);
      const th1 = await core.getThemes(id);
      expect(th1.active).toBe("ocean");

      expect(await core.selectTheme(id, "neon", 0)).toBe(true);
      const th2 = await core.getThemes(id);
      expect(th2.strips[0]).toBe("neon");

      expect(await core.selectTheme(id, "nope")).toBe(false);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("setStripEffect changes the effect on strip 0", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-d3-fx-"));
    const core = new AppCore({ dataDir: dir, appInfo: { version: "0.0.0-test" } });
    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;

      expect(await core.setStripEffect(id, 0, 7)).toBe(true);
      const st = await core.getState(id);
      expect(st.effect[0]).toBe(7);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("selectAudioSource changes the active source", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-d3-audio-"));
    const core = new AppCore({ dataDir: dir, appInfo: { version: "0.0.0-test" } });
    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;

      expect(await core.selectAudioSource(id, 2)).toBe(true);
      const src = await core.getAudioSources(id);
      expect(src.active).toBe(2);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("setAutoSelect toggles auto-select on and off", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-d3-auto-"));
    const core = new AppCore({ dataDir: dir, appInfo: { version: "0.0.0-test" } });
    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;

      expect(await core.setAutoSelect(id, true)).toBe(true);
      const src = await core.getAudioSources(id);
      expect(src.autoSelect).toBe(true);

      expect(await core.setAutoSelect(id, false)).toBe(true);
      const src2 = await core.getAudioSources(id);
      expect(src2.autoSelect).toBe(false);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("triggerCinematicTest returns ok", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "resonux-d3-cin-"));
    const core = new AppCore({ dataDir: dir, appInfo: { version: "0.0.0-test" } });
    try {
      await waitFor(() => core.snapshot().controllers.some((c) => c.kind === "simulator" && c.online));
      const id = core.snapshot().controllers.find((c) => c.kind === "simulator")!.id;
      expect(await core.triggerCinematicTest(id)).toBe(true);
    } finally {
      core.stop();
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });
});