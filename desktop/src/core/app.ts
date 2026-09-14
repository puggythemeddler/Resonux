// AppCore — the sole owner of desktop-app state in the main process. It wires
// the registry (discovery + health), the simulator, and persisted settings
// together and exposes the command surface the renderer calls over IPC.

import { ControllerRegistry } from "../client/registry";
import { get, post, HttpError } from "../client/http";
import { MockController } from "../sim/mock";
import { SettingsStore } from "./settings";
import type {
  AppInfo,
  AppSettings,
  Snapshot,
  StatusSnapshot,
  CinematicPayload,
  DevicesPayload,
  AudioSourcesPayload,
  ThemeMode,
} from "../domain/bridge";
import type { HttpEndpoint } from "../client/http";

type Listener = () => void;

export interface AppCoreOptions {
  dataDir: string;
  appInfo: Omit<AppInfo, "platform" | "isPackaged">;
  simulatorEnabled?: boolean;
}

export class AppCore {
  private readonly registry: ControllerRegistry;
  private readonly settings: SettingsStore;
  private readonly listeners = new Set<Listener>();
  private simulator: MockController | null = null;
  private simulatorRunning = true;
  private readonly appInfo: AppInfo;
  private lastEmit = 0;

  constructor(opts: AppCoreOptions) {
    const defaults = { ...opts, simulatorEnabled: opts.simulatorEnabled ?? true };
    this.settings = new SettingsStore(opts.dataDir);
    this.simulatorRunning = this.settings.get().simulatorRunning;
    this.appInfo = {
      version: opts.appInfo.version,
      platform: process.platform,
      isPackaged: !process.argv.filter((a) => a.includes("electron")).length,
    };

    this.registry = new ControllerRegistry({ pollMs: 2000 });
    this.registry.onChange(() => this.maybeEmit());

    if (this.simulatorRunning) void this.startSimulatorInternal();
    this.registry.startScanning();
  }

  stop(): void {
    this.registry.stop();
    void this.shutdownSimulator();
  }

  onChange(fn: Listener): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private emit(): void {
    for (const fn of this.listeners) fn();
  }

  private maybeEmit(): void {
    const now = Date.now();
    if (now - this.lastEmit >= 200 || this.lastEmit === 0) {
      this.lastEmit = now;
      this.emit();
    } else {
      // Coalesce bursty health-poll updates into at most ~5 Hz pushes.
      setTimeout(() => {
        this.lastEmit = Date.now();
        this.emit();
      }, 220);
    }
  }

  // ---------------------------------------------------------- queries

  info(): AppInfo {
    return this.appInfo;
  }

  snapshot(): Snapshot {
    return this.registry.snapshot(
      this.settings.get().selectedControllerId,
      this.settings.get(),
      this.simulatorRunning
    );
  }

  // ---------------------------------------------------------- settings

  setTheme(mode: ThemeMode): void {
    this.settings.patch({ theme: mode });
    this.emit();
  }

  // ------------------------------------------------------ controllers

  selectController(id: string | null): void {
    if (id === null) {
      this.settings.patch({ selectedControllerId: null });
      this.emit();
      return;
    }
    if (!this.registry.has(id)) return;
    this.settings.patch({ selectedControllerId: id });
    this.emit();
  }

  rescan(): void {
    this.registry.scanNow();
    this.emit();
  }

  setSimulator(enabled: boolean): void {
    this.simulatorRunning = enabled;
    this.settings.patch({ simulatorRunning: enabled });
    if (enabled) void this.startSimulatorInternal();
    else void this.shutdownSimulator();
    this.emit();
  }

  private async startSimulatorInternal(): Promise<void> {
    if (this.simulator) return;
    const sim = new MockController({ name: "Demo Room", id: "sim-demo" });
    try {
      const port = await sim.listen();
      this.simulator = sim;
      this.registry.registerSimulator("sim-demo", sim.name, port);
    } catch {
      // If the loopback server can't bind, run without a simulator.
    }
  }

  private async shutdownSimulator(): Promise<void> {
    const sim = this.simulator;
    this.simulator = null;
    this.registry.removeSimulator("sim-demo");
    if (sim) await sim.close();
  }

  // --------------------------------------------------- device commands

  private endpointFor(id: string): HttpEndpoint | null {
    const info = this.registry.list().find((c) => c.id === id);
    return info ? { address: info.address, port: info.port } : null;
  }

  async getStatus(id: string): Promise<StatusSnapshot> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    return get<StatusSnapshot>(ep, "/api/status");
  }

  async getCinematic(id: string): Promise<CinematicPayload> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    return get<CinematicPayload>(ep, "/api/cinematic");
  }

  async getDevices(id: string): Promise<DevicesPayload> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    return get<DevicesPayload>(ep, "/api/devices");
  }

  async getAudioSources(id: string): Promise<AudioSourcesPayload> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    return get<AudioSourcesPayload>(ep, "/api/audio/sources");
  }

  async setBrightness(id: string, value: number): Promise<boolean> {
    const ep = this.endpointFor(id);
    if (!ep) return false;
    const v = Math.max(0, Math.min(255, Math.round(value)));
    const res = await post<{ ok?: boolean }>(ep, "/api/state/brightness", { value: v });
    return res?.ok === true;
  }

  async setCinematic(id: string, enabled: boolean): Promise<boolean> {
    const ep = this.endpointFor(id);
    if (!ep) return false;
    const res = await post<{ ok?: boolean }>(ep, "/api/cinematic", { enabled });
    return res?.ok === true;
  }
}