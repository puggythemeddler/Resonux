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