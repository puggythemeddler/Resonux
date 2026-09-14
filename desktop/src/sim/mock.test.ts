import { describe, it, expect, afterAll } from "vitest";
import { MockController } from "./mock";
import type { StatusSnapshot, CinematicPayload } from "../domain/types";

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