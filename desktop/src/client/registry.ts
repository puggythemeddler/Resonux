// Controller registry: keeps the set of known controllers (discovered over
// UDP or registered as local simulators), health-checks each over its REST
// surface, and renders the snapshot the UI presents. Entry lifecycle mirrors
// the firmware DeviceManager: last-seen timestamps, lenient parsing, and
// offline state that preserves what we knew before it went away.

import { performance } from "node:perf_hooks";
import { get, HttpError, type HttpEndpoint } from "./http";
import { UdpDiscovery, type DiscoveredController } from "./discovery";
import { type StatusSnapshot } from "../domain/types";
import type { AppSettings, ControllerInfo, Snapshot } from "../domain/bridge";

type Listener = () => void;

interface Entry {
  info: ControllerInfo;
  failures: number;
  status: StatusSnapshot | null;
}

const DEFAULT_HTTP_PORT = 80;

function endpointFor(info: ControllerInfo): HttpEndpoint {
  return { address: info.address, port: info.port };
}

export interface RegistryOptions {
  pollMs?: number;
  offlineAfterFailures?: number;
  httpTimeoutMs?: number;
}

export class ControllerRegistry {
  private readonly entries = new Map<string, Entry>();
  private readonly byAddress = new Map<string, string>(); // address -> id
  private readonly listeners = new Set<Listener>();
  private readonly discovery: UdpDiscovery;
  private pollTimer: NodeJS.Timeout | null = null;
  lastScanMs = 0;
  private readonly opts: Required<RegistryOptions>;

  constructor(opts: RegistryOptions = {}) {
    this.opts = {
      pollMs: opts.pollMs ?? 2000,
      offlineAfterFailures: opts.offlineAfterFailures ?? 2,
      httpTimeoutMs: opts.httpTimeoutMs ?? 3000,
    };
    this.discovery = new UdpDiscovery();
  }

  startScanning(): void {
    this.discovery.start((c) => this.onDiscovered(c));
    this.ensurePollLoop();
  }

  stop(): void {
    this.discovery.stop();
    if (this.pollTimer) {
      clearInterval(this.pollTimer);
      this.pollTimer = null;
    }
  }

  private ensurePollLoop(): void {
    if (!this.pollTimer) {
      this.pollTimer = setInterval(() => void this.poll(), this.opts.pollMs);
    }
  }

  scanNow(): void {
    this.discovery.ping();
  }

  onChange(fn: Listener): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private emit(): void {
    for (const fn of this.listeners) fn();
  }

  registerSimulator(id: string, name: string, port: number): void {
    const info: ControllerInfo = {
      id,
      name,
      kind: "simulator",
      address: "127.0.0.1",
      port,
      fw: "sim-0.1",
      role: "master",
      caps: 0,
      lastSeenMs: Date.now(),
      online: false,
      health: "offline",
    };
    this.entries.set(id, { info, failures: 0, status: null });
    this.byAddress.set(info.address, id);
    this.ensurePollLoop();
    this.emit();
    void this.pollEntry(id);
  }

  removeSimulator(id: string): void {
    const entry = this.entries.get(id);
    if (entry?.info.kind === "simulator") {
      this.entries.delete(id);
      this.byAddress.delete(entry.info.address);
      this.emit();
    }
  }

  private onDiscovered(c: DiscoveredController): void {
    this.lastScanMs = Date.now();
    const existing = this.byAddress.get(c.address);
    if (existing && this.entries.has(existing)) {
      const e = this.entries.get(existing)!;
      e.info.lastSeenMs = Date.now();
      e.info.fw = c.fw || e.info.fw;
      e.info.role = c.role || e.info.role;
      e.info.caps = c.caps || e.info.caps;
      this.emit();
      return;
    }
    const info: ControllerInfo = {
      id: c.id,
      name: c.name || c.id,
      kind: "discover",
      address: c.address,
      port: DEFAULT_HTTP_PORT,
      fw: c.fw,
      role: c.role,
      caps: c.caps,
      lastSeenMs: Date.now(),
      online: false,
      health: "offline",
    };
    this.entries.set(info.id, { info, failures: 0, status: null });
    this.byAddress.set(info.address, info.id);
    this.emit();
    void this.pollEntry(info.id);
  }

  list(): ControllerInfo[] {
    return [...this.entries.values()]
      .map((e) => e.info)
      .sort((a, b) => {
        const rank = (i: ControllerInfo) => (i.online ? 0 : 1) + (i.kind === "simulator" ? 1 : 0);
        return rank(a) - rank(b) || a.name.localeCompare(b.name);
      });
  }

  statusOf(id: string): StatusSnapshot | null {
    return this.entries.get(id)?.status ?? null;
  }

  has(id: string): boolean {
    return this.entries.has(id);
  }

  private async pollEntry(id: string): Promise<void> {
    const entry = this.entries.get(id);
    if (!entry) return;
    const ep = endpointFor(entry.info);
    const t0 = performance.now();
    try {
      const status = await get<StatusSnapshot>(ep, "/api/status", {
        timeoutMs: this.opts.httpTimeoutMs,
      });
      if (!status || status.ok === false) throw new HttpError(0, "bad", "");
      entry.status = status;
      entry.info.online = true;
      entry.info.health = status.heap < 8192 ? "degraded" : "ok";
      entry.info.latencyMs = Math.round(performance.now() - t0);
      entry.info.name = status.device || entry.info.name;
      entry.info.uptimeMs = status.uptimeMs;
      entry.info.fps = status.fps;
      entry.info.lastSeenMs = Date.now();
      entry.failures = 0;
    } catch {
      entry.failures += 1;
      if (entry.failures >= this.opts.offlineAfterFailures) {
        entry.info.online = false;
        entry.info.health = "offline";
        // Stale stats next to an Offline chip would lie; identity stays.
        entry.status = null;
      } else {
        entry.info.health = "degraded";
      }
    }
    this.emit();
  }

  private async poll(): Promise<void> {
    const ids = [...this.entries.keys()];
    await Promise.all(ids.map((id) => this.pollEntry(id)));
  }

  snapshot(selectedId: string | null, settings: AppSettings, simulatorRunning: boolean): Snapshot {
    const controllers = this.list();
    const selectedInfo = controllers.find((c) => c.id === selectedId) ?? null;
    const st = selectedInfo ? this.statusOf(selectedInfo.id) : null;
    return {
      controllers,
      selectedId: selectedInfo?.id ?? null,
      selected: selectedInfo && st ? { ...selectedInfo, status: st } : null,
      simulatorRunning,
      settings,
      lastScanMs: this.lastScanMs,
    };
  }
}