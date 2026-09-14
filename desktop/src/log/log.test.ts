import { describe, it, expect } from "vitest";
import { LogStore } from "./log";

describe("LogStore", () => {
  it("records events in order with increasing seq and ISO timestamps", () => {
    const log = new LogStore();
    log.add({ level: "info", kind: "app", message: "first" });
    log.add({ level: "warn", kind: "system", message: "second", controllerId: "sim-demo", controllerName: "Demo Room" });

    const entries = log.entries();
    expect(entries).toHaveLength(2);
    expect(entries[0].seq).toBe(0);
    expect(entries[1].seq).toBe(1);
    expect(entries[1].message).toBe("second");
    expect(entries[1].controllerName).toBe("Demo Room");
    expect(/^\d{4}-\d{2}-\d{2}T/.test(entries[0].at)).toBe(true);
  });

  it("caps the buffer at the configured size, keeping the newest events", () => {
    const log = new LogStore(5);
    for (let i = 0; i < 20; i++) log.add({ level: "info", kind: "app", message: `event ${i}` });

    const entries = log.entries();
    expect(entries).toHaveLength(5);
    expect(entries[0].seq).toBe(15);
    expect(entries[entries.length - 1].seq).toBe(19);
  });

  it("returns copies, so callers cannot mutate the store", () => {
    const log = new LogStore();
    log.add({ level: "info", kind: "app", message: "original" });
    log.entries()[0].message = "tampered";
    expect(log.entries()[0].message).toBe("original");
  });
});