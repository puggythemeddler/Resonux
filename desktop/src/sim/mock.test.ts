import { describe, it, expect, afterAll } from "vitest";
import { MockController } from "./mock";
import type { StatusSnapshot, CinematicPayload, ThemesPayload, AudioSourcesPayload, StatePayload } from "../domain/types";

describe("MockController REST surface", () => {
  const mock = new MockController({ name: "Lab", id: "sim-lab" });

  it("serves /api/status with the firmware shape", async () => {
    const port = await mock.listen();
    const res = await fetch(`http://127.0.0.1:${port}/api/status`);
    expect(res.status).toBe(200);
    const body = (await res.json()) as StatusSnapshot;
    expect(body.ok).toBe(true);
    expect(body.device).toBe("Simulator / Demo");
    expect(typeof body.fps).toBe("number");
    expect(body.wifi.ip).toBe("127.0.0.1");
    expect(body.sync.role).toBe("master");
    expect(body.system.state).toBe("reactive");
  });

  it("mutates brightness through the state endpoint", async () => {
    const port = mock.portNumber;
    const set = await fetch(`http://127.0.0.1:${port}/api/state/brightness`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ value: 77 }),
    });
    expect(set.status).toBe(200);
    const state = (await (await fetch(`http://127.0.0.1:${port}/api/state`)).json()) as {
      brightness: number;
    };
    expect(state.brightness).toBe(77);

    const bad = await fetch(`http://127.0.0.1:${port}/api/state/brightness`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ value: 300 }),
    });
    expect(bad.status).toBe(400);
  });

  it("serves the cinematic payload and honours the toggle", async () => {
    const port = mock.portNumber;
    const before = (await (await fetch(`http://127.0.0.1:${port}/api/cinematic`)).json()) as CinematicPayload;
    expect(before.active).toBe(true);
    expect(before.status.sceneId).toBe("action");

    await fetch(`http://127.0.0.1:${port}/api/cinematic`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ enabled: false }),
    });
    const after = (await (await fetch(`http://127.0.0.1:${port}/api/cinematic`)).json()) as CinematicPayload;
    expect(after.active).toBe(false);
    expect(after.status.sceneId).toBe("off");
  });

  it("404s on unknown routes", async () => {
    const port = mock.portNumber;
    const res = await fetch(`http://127.0.0.1:${port}/api/nope`);
    expect(res.status).toBe(404);
  });

  afterAll(async () => {
    await mock.close();
  });
});

describe("MockController system commands", () => {
  it("accepts a restart, drops the port, and comes back on the SAME port", async () => {
    const m = new MockController({ id: "sim-rs" });
    try {
      const port = await m.listen();
      const res = await fetch(`http://127.0.0.1:${port}/api/system/restart`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: "{}",
      });
      expect(res.status).toBe(200);
      expect(await res.json()).toMatchObject({ ok: true, action: "restart" });

      // The simulated reboot gap is real: watchers observe the outage, then the
      // same loopback port answers again with normal system state.
      const deadline = Date.now() + 4000;
      let back = false;
      while (Date.now() < deadline) {
        try {
          const status = (await (await fetch(`http://127.0.0.1:${port}/api/status`)).json()) as StatusSnapshot;
          back = status.system.state === "reactive";
          if (back) break;
        } catch {
          await new Promise((r) => setTimeout(r, 40));
        }
        await new Promise((r) => setTimeout(r, 40));
      }
      expect(back).toBe(true);
    } finally {
      await m.close();
    }
  });

  it("power-off replies ok and honestly reports the sleeping state", async () => {
    const m = new MockController({ id: "sim-po" });
    try {
      const port = await m.listen();
      const res = await fetch(`http://127.0.0.1:${port}/api/system/power-off`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: "{}",
      });
      expect(res.status).toBe(200);
      expect(await res.json()).toMatchObject({ ok: true, action: "power_off" });

      const status = (await (await fetch(`http://127.0.0.1:${port}/api/status`)).json()) as StatusSnapshot;
      expect(status.system.state).toBe("sleeping");
      expect(status.system.action).toBe("power_off");
    } finally {
      await m.close();
    }
  });
});

describe("MockController config writes", () => {
  it("renames the device through PUT /api/config", async () => {
    const m = new MockController({ id: "sim-cfg" });
    try {
      const port = await m.listen();
      const put = await fetch(`http://127.0.0.1:${port}/api/config`, {
        method: "PUT",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ deviceName: "Living Room" }),
      });
      expect(put.status).toBe(200);
      const status = (await (await fetch(`http://127.0.0.1:${port}/api/status`)).json()) as StatusSnapshot;
      expect(status.device).toBe("Living Room");
      const cfg = (await (await fetch(`http://127.0.0.1:${port}/api/config`)).json()) as {
        deviceName: string;
      };
      expect(cfg.deviceName).toBe("Living Room");
    } finally {
      await m.close();
    }
  });

  it("simulates the Wi-Fi hand-off: ap mode joins a network and reports it honestly", async () => {
    const m = new MockController({ id: "sim-ap", apMode: true });
    try {
      const port = await m.listen();
      const before = (await (await fetch(`http://127.0.0.1:${port}/api/status`)).json()) as StatusSnapshot;
      expect(before.wifi.mode).toBe("ap");
      expect(before.wifi.connected).toBe(false);
      expect(before.wifi.ip).toBe("192.168.4.1");

      const put = await fetch(`http://127.0.0.1:${port}/api/config`, {
        method: "PUT",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ net: { staSsid: "HomeNet", staPassword: "hunter2" } }),
      });
      expect(put.status).toBe(200);
      expect(await put.json()).toEqual({ ok: true, rebooting: false });

      const after = (await (await fetch(`http://127.0.0.1:${port}/api/status`)).json()) as StatusSnapshot;
      expect(after.wifi.mode).toBe("sta");
      expect(after.wifi.connected).toBe(true);
      expect(after.wifi.ip).toBe("192.168.4.100");

      const cfg = (await (await fetch(`http://127.0.0.1:${port}/api/config`)).json()) as {
        net: { staSsid: string; staPassword: string; mode: number };
      };
      expect(cfg.net.staSsid).toBe("HomeNet");
      expect(cfg.net.staPassword).toBe("hunter2");
      expect(cfg.net.mode).toBe(1);
    } finally {
      await m.close();
    }
  });

  it("restarts on the same address and keeps state", async () => {
    const m = new MockController({ id: "sim-rs", name: "Restart me" });
    try {
      const port = await m.listen();
      await fetch(`http://127.0.0.1:${port}/api/config`, {
        method: "PUT",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ deviceName: "New" }),
      });
      await m.restart();
      expect(m.portNumber).toBe(port);
      const status = (await (await fetch(`http://127.0.0.1:${port}/api/status`)).json()) as StatusSnapshot;
      expect(status.device).toBe("New");
      expect(status.uptimeMs).toBeLessThan(1000); // uptime reset by the "reboot"
    } finally {
      await m.close();
    }
  });
});

// ---------- D3: themes, effect-per-strip, audio source, cinematic test -----

describe("MockController themes and D3 surface", () => {
  it("serves a themes payload with built-ins and strips", async () => {
    const m = new MockController({ id: "sim-themes" });
    try {
      await m.listen();
      const res = await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`);
      expect(res.status).toBe(200);
      const t = (await res.json()) as ThemesPayload;
      expect(t.themes.length).toBeGreaterThanOrEqual(2);
      expect(t.themes.map((th) => th.id)).toContain("classic");
      expect(typeof t.active).toBe("string");
      expect(Array.isArray(t.strips)).toBe(true);
    } finally {
      await m.close();
    }
  });

  it("selects a global theme and a per-strip theme", async () => {
    const m = new MockController({ id: "sim-thsel" });
    try {
      await m.listen();
      const post = async (body: Record<string, unknown>) =>
        (await fetch(`http://127.0.0.1:${m.portNumber}/api/themes/select`, {
          method: "POST",
          headers: { "content-type": "application/json" },
          body: JSON.stringify(body),
        })).json();

      await post({ id: "ocean" });
      const g = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`)).json()) as ThemesPayload;
      expect(g.active).toBe("ocean");

      await post({ id: "neon", strip: 0 });
      const s = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`)).json()) as ThemesPayload;
      expect(s.strips[0]).toBe("neon");

      const bad = await post({ id: "nope" });
      expect((bad as { error?: string }).error).toBe("unknown theme");
    } finally {
      await m.close();
    }
  });

  it("PUTs a new theme and deletes it", async () => {
    const m = new MockController({ id: "sim-thput" });
    try {
      await m.listen();
      const u = await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`, {
        method: "PUT",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({
          id: "custom1",
          name: "Custom",
          palette: ["#112233"],
          brightness: { base: 50, min: 10 },
          saturation: 100,
          response: { bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1 },
          animation: { movement: 0.5, pulse: 0.5, flash: 0.5, sparkle: 0.5, smoothing: 0.5, contrast: 0.5, density: 0.5 },
          effects: [1],
        }),
      });
      expect(u.status).toBe(200);
      const after = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`)).json()) as ThemesPayload;
      expect(after.themes.some((t) => t.id === "custom1")).toBe(true);

      const d = await fetch(`http://127.0.0.1:${m.portNumber}/api/themes?id=custom1`, { method: "DELETE" });
      expect(d.status).toBe(200);
      const gone = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`)).json()) as ThemesPayload;
      expect(gone.themes.some((t) => t.id === "custom1")).toBe(false);

      const badDel = await fetch(`http://127.0.0.1:${m.portNumber}/api/themes?id=classic`, { method: "DELETE" });
      expect(badDel.status).toBe(400);
    } finally {
      await m.close();
    }
  });

  it("resets themes to the built-in defaults", async () => {
    const m = new MockController({ id: "sim-thrst" });
    try {
      await m.listen();
      await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`, {
        method: "PUT",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({
          id: "tmp", name: "Tmp", palette: ["#aaa"], brightness: { base: 30, min: 5 },
          saturation: 50, response: { bass: 1, lowMid: 1, mid: 1, highMid: 1, treble: 1, beat: 1, amp: 1 },
          animation: { movement: 0, pulse: 0, flash: 0, sparkle: 0, smoothing: 0, contrast: 0, density: 0 }, effects: [],
        }),
      });
      const pre = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`)).json()) as ThemesPayload;
      expect(pre.themes.some((t) => t.id === "tmp")).toBe(true);

      await fetch(`http://127.0.0.1:${m.portNumber}/api/themes/reset`, { method: "POST" });
      const post = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/themes`)).json()) as ThemesPayload;
      expect(post.themes.some((t) => t.id === "tmp")).toBe(false);
      expect(post.themes.map((t) => t.id)).toContain("classic");
      expect(post.strips.every((s) => s === "")).toBe(true);
    } finally {
      await m.close();
    }
  });

  it("sets per-strip effect", async () => {
    const m = new MockController({ id: "sim-fx" });
    try {
      await m.listen();
      const bad = await fetch(`http://127.0.0.1:${m.portNumber}/api/state/effect`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ strip: 0, effectId: 99 }),
      });
      expect(bad.status).toBe(400);

      const ok = await fetch(`http://127.0.0.1:${m.portNumber}/api/state/effect`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ strip: 0, effectId: 7 }),
      });
      expect(ok.status).toBe(200);
      const state = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/state`)).json()) as StatePayload;
      expect(state.effect[0]).toBe(7);
    } finally {
      await m.close();
    }
  });

  it("selects audio source and toggles auto-select", async () => {
    const m = new MockController({ id: "sim-audio" });
    try {
      await m.listen();
      const pick = await fetch(`http://127.0.0.1:${m.portNumber}/api/audio/source`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ source: 2 }),
      });
      expect(pick.status).toBe(200);
      const r1 = (await pick.json()) as { ok: boolean; source: number; autoSelect: boolean };
      expect(r1.ok).toBe(true);
      expect(r1.source).toBe(2);
      expect(r1.autoSelect).toBe(false);

      const auto = await fetch(`http://127.0.0.1:${m.portNumber}/api/audio/source`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ autoSelect: true }),
      });
      const r2 = (await auto.json()) as { source: number; autoSelect: boolean };
      expect(r2.source).toBe(2);
      expect(r2.autoSelect).toBe(true);

      const src = (await (await fetch(`http://127.0.0.1:${m.portNumber}/api/audio/sources`)).json()) as AudioSourcesPayload;
      expect(src.active).toBe(2);
      expect(src.autoSelect).toBe(true);
    } finally {
      await m.close();
    }
  });

  it("accepts a cinematic test burst", async () => {
    const m = new MockController({ id: "sim-cin-test" });
    try {
      await m.listen();
      const res = await fetch(`http://127.0.0.1:${m.portNumber}/api/cinematic/test`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({}),
      });
      expect(res.status).toBe(200);
      const body = (await res.json()) as { ok: boolean };
      expect(body.ok).toBe(true);
    } finally {
      await m.close();
    }
  });
});