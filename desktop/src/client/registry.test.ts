import { describe, it, expect, vi, afterEach } from "vitest";
import { ControllerRegistry } from "./registry";
import { MockController } from "../sim/mock";

async function waitFor(
  fn: () => boolean,
  timeoutMs = 4000,
  stepMs = 30
): Promise<void> {
  const start = Date.now();
  while (!fn()) {
    if (Date.now() - start > timeoutMs) throw new Error("condition not met in time");
    await new Promise((r) => setTimeout(r, stepMs));
  }
}

describe("ControllerRegistry", () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("heals a simulator entry online and exposes its status in snapshots", async () => {
    const mock = new MockController({ id: "sim-t1", name: "Test Room" });
    await mock.listen();
    const registry = new ControllerRegistry({ pollMs: 40, offlineAfterFailures: 1, httpTimeoutMs: 1000 });
    const onChange = vi.fn();
    registry.onChange(onChange);
    registry.registerSimulator("sim-t1", "Test Room", mock.portNumber);

    await waitFor(() => {
      const info = registry.list().find((c) => c.id === "sim-t1");
      return info?.online === true;
    });

    const info = registry.list().find((c) => c.id === "sim-t1")!;
    expect(info.health).toBe("ok");
    expect(typeof info.latencyMs).toBe("number");
    expect(registry.statusOf("sim-t1")?.device).toBe("Simulator / Demo");

    const snap = registry.snapshot("sim-t1", { theme: "dark", selectedControllerId: "sim-t1", simulatorRunning: true, wizardCompleted: false }, true);
    expect(snap.selected?.status.fps).toBeGreaterThan(0);
    expect(snap.selected?.kind).toBe("simulator");
    expect(onChange.mock.calls.length).toBeGreaterThan(0);

    registry.stop();
    await mock.close();
  });

  it("marks an entry offline once the controller stops answering", async () => {
    const mock = new MockController({ id: "sim-t2" });
    await mock.listen();
    const registry = new ControllerRegistry({ pollMs: 40, offlineAfterFailures: 1, httpTimeoutMs: 500 });
    registry.registerSimulator("sim-t2", "Ephemeral", mock.portNumber);

    await waitFor(() => registry.list().some((c) => c.id === "sim-t2" && c.online));
    await mock.close();

    await waitFor(() => {
      const info = registry.list().find((c) => c.id === "sim-t2");
      return info?.health === "offline";
    });
    expect(registry.list().find((c) => c.id === "sim-t2")!.online).toBe(false);
    // Last known identity survives (device-reported name), but status is gone.
    const entry = registry.list().find((c) => c.id === "sim-t2")!;
    expect(entry.name).toBe("Simulator / Demo");
    expect(registry.statusOf("sim-t2")).toBeNull();

    registry.stop();
  });

  it("comes back online after a keep-port restart (config-reboot path)", async () => {
    const mock = new MockController({ id: "sim-t4" });
    await mock.listen();
    const registry = new ControllerRegistry({ pollMs: 40, offlineAfterFailures: 1, httpTimeoutMs: 300 });
    registry.registerSimulator("sim-t4", "Flicker", mock.portNumber);

    await waitFor(() => registry.list().some((c) => c.id === "sim-t4" && c.online));
    await mock.restart();

    // First it notices the outage…
    await waitFor(() => registry.list().find((c) => c.id === "sim-t4")?.health === "offline");
    expect(registry.statusOf("sim-t4")).toBeNull();

    // …then the registry discovers it is answering again on the same address.
    await waitFor(() => registry.list().find((c) => c.id === "sim-t4")?.online === true);
    expect(registry.statusOf("sim-t4")).not.toBeNull();

    registry.stop();
    await mock.close();
  });

  it("removes a simulator and stops reporting it", async () => {
    const registry = new ControllerRegistry({ pollMs: 40 });
    registry.registerSimulator("sim-t3", "Gone", 1);
    registry.removeSimulator("sim-t3");
    expect(registry.list()).toHaveLength(0);
    registry.stop();
  });
});