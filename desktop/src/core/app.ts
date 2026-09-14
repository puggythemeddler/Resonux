// AppCore — the sole owner of desktop-app state in the main process. It wires
// the registry (discovery + health), the simulator, and persisted settings
// together and exposes the command surface the renderer calls over IPC.

import { ControllerRegistry } from "../client/registry";
import { get, getText, post, put, HttpError, describeError } from "../client/http";
import { MockController } from "../sim/mock";
import fs from "node:fs";
import path from "node:path";
import { SettingsStore } from "./settings";
import type {
  AppInfo,
  AppSettings,
  Snapshot,
  StatusSnapshot,
  CinematicPayload,
  DevicesPayload,
  AudioSourcesPayload,
  ThemesPayload,
  ThemeMode,
  ConfigCommandResult,
  HardwareCheckReport,
} from "../domain/bridge";
import type { ConfigPayload, FrameSample, StatePayload } from "../domain/types";
import type { HttpEndpoint } from "../client/http";
import { runHardwareCheck, type CheckCalls } from "../diagnostics/check";

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
    const settings = this.settings.get();
    return {
      ...this.registry.snapshot(settings.selectedControllerId, settings, this.simulatorRunning),
      wizardNeeded: !settings.wizardCompleted && settings.selectedControllerId === null,
    };
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
    if (!ep) return Promise.resolve(false);
    return post<{ ok?: boolean }>(ep, "/api/cinematic", { enabled }).then(
      (res) => res?.ok === true,
      () => false
    );
  }

  // ----------------------------------------------------- config writes

  // Byte-exact snapshot of the current config, parked in userData/backups
  // before any config-writing command. The path is surfaced so recovery can
  // replay it later; the file itself is never read here.
  private backupConfig(ep: HttpEndpoint, id: string): string | null {
    try {
      const safeId = id.replace(/[^a-zA-Z0-9_-]/g, "-");
      const dir = path.join(this.settings.dataDir, "backups");
      fs.mkdirSync(dir, { recursive: true });
      void getText(ep, "/api/config").then((text) => {
        const file = path.join(dir, `config-${safeId}-${Date.now()}.json`);
        try {
          fs.writeFileSync(file, text, "utf8");
        } catch {
          // Backup is best-effort; the command itself still proceeds.
        }
      });
      return dir;
    } catch {
      return null;
    }
  }

  async renameController(id: string, name: string): Promise<ConfigCommandResult> {
    const ep = this.endpointFor(id);
    if (!ep) return { ok: false, rebootApplied: false, backupPath: null, detail: `Unknown controller: ${id}` };
    const trimmed = name.trim();
    if (!trimmed) return { ok: false, rebootApplied: false, backupPath: null, detail: "Name must not be empty" };
    try {
      const backupPath = this.backupConfig(ep, id);
      // GET-then-PUT round-trips the full config document: the firmware stores
      // whatever the PUT body is verbatim, so unknown keys survive untouched.
      const cfg = await get<ConfigPayload>(ep, "/api/config");
      const merged = { ...cfg, deviceName: trimmed };
      await put(ep, "/api/config", merged);
      return { ok: true, rebootApplied: true, backupPath };
    } catch (err) {
      return { ok: false, rebootApplied: false, backupPath: null, detail: describeError(err) };
    }
  }

  async setWifi(id: string, ssid: string, password: string): Promise<ConfigCommandResult> {
    const ep = this.endpointFor(id);
    if (!ep) return { ok: false, rebootApplied: false, backupPath: null, detail: `Unknown controller: ${id}` };
    const trimmedSsid = ssid.trim();
    if (!trimmedSsid) return { ok: false, rebootApplied: false, backupPath: null, detail: "Wi-Fi name must not be empty" };
    // Password never leaves this method — not logged, not persisted.
    try {
      const backupPath = this.backupConfig(ep, id);
      const cfg = await get<ConfigPayload>(ep, "/api/config");
      const net = cfg.net ?? { enabled: true, mode: 0, apSsid: "Resonux", apPassword: "", staSsid: "", staPassword: "" };
      const merged: ConfigPayload = { ...cfg, net: { ...net, staSsid: trimmedSsid, staPassword: password } };
      await put(ep, "/api/config", merged);
      return { ok: true, rebootApplied: true, wifiApplied: true, backupPath };
    } catch (err) {
      return { ok: false, rebootApplied: false, backupPath: null, detail: describeError(err) };
    }
  }

  // Polls the registry (which is already health-checking every entry) until
  // the controller is answering again. Used after a config write that reboots
  // the unit — honest timeout, never an infinite spinner.
  async waitOnline(id: string, timeoutMs: number): Promise<boolean> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const c = this.registry.list().find((x) => x.id === id);
      if (c?.online && c.health === "ok") return true;
      await new Promise((r) => setTimeout(r, 250));
    }
    return false;
  }

  finishWizard(): void {
    this.settings.patch({ wizardCompleted: true });
    this.emit();
  }

  // ----------------------------------------------------- D3: control surface

  async getState(id: string): Promise<StatePayload> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    return get<StatePayload>(ep, "/api/state");
  }

  async getFrame(id: string): Promise<FrameSample> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    return get<FrameSample>(ep, "/api/frame");
  }

  async getThemes(id: string): Promise<ThemesPayload> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    return get<ThemesPayload>(ep, "/api/themes");
  }

  private async okCommand(ep: HttpEndpoint | null, path: string, body?: unknown): Promise<boolean> {
    if (!ep) return false;
    try {
      const res = await post<{ ok?: boolean }>(ep, path, body);
      return res?.ok === true;
    } catch {
      return false;
    }
  }

  async selectTheme(id: string, themeId: string, strip?: number): Promise<boolean> {
    const ep = this.endpointFor(id);
    return this.okCommand(ep, "/api/themes/select", { id: themeId, ...(strip === undefined ? {} : { strip }) });
  }

  async setStripEffect(id: string, strip: number, effectId: number): Promise<boolean> {
    const ep = this.endpointFor(id);
    return this.okCommand(ep, "/api/state/effect", { strip, effectId });
  }

  async selectAudioSource(id: string, source: number): Promise<boolean> {
    const ep = this.endpointFor(id);
    return this.okCommand(ep, "/api/audio/source", { source });
  }

  async setAutoSelect(id: string, autoSelect: boolean): Promise<boolean> {
    const ep = this.endpointFor(id);
    return this.okCommand(ep, "/api/audio/source", { autoSelect });
  }

  async triggerCinematicTest(id: string): Promise<boolean> {
    const ep = this.endpointFor(id);
    // Empty body: the firmware fills in defaults for the QA burst.
    return this.okCommand(ep, "/api/cinematic/test", {});
  }

  // ------------------------------------------------------ D4: hardware check

  async runHardwareCheck(id: string): Promise<HardwareCheckReport> {
    const ep = this.endpointFor(id);
    if (!ep) throw new HttpError(404, "unknown controller", id);
    const info = this.registry.list().find((c) => c.id === id);
    const calls: CheckCalls = {
      fetchStatus: () => get<StatusSnapshot>(ep, "/api/status"),
      fetchFrame: () => get<FrameSample>(ep, "/api/frame"),
      fetchState: () => get<StatePayload>(ep, "/api/state"),
      setBrightness: (value) => this.okCommand(ep, "/api/state/brightness", { value }),
    };
    return runHardwareCheck(
      {
        id,
        name: info?.name ?? id,
        isSimulator: info?.kind === "simulator",
      },
      calls
    );
  }
}