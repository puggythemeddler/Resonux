// Contract between the Electron main process and the React renderer. The
// renderer talks only to this surface through contextBridge — structured
// data, no Node APIs, no closures.

import type { StatusSnapshot, CinematicPayload, DevicesPayload, AudioSourcesPayload, ThemesPayload, StatePayload, AudioSourceOption, ThemeDefWire, HardwareCheckReport, CheckStepResult, CheckVerdict } from "./types";

export type {
  StatusSnapshot,
  CinematicPayload,
  DevicesPayload,
  AudioSourcesPayload,
  ThemesPayload,
  StatePayload,
  AudioSourceOption,
  ThemeDefWire,
  HardwareCheckReport,
  CheckStepResult,
  CheckVerdict,
};

export type ThemeMode = "system" | "dark" | "light";
export type ControllerKind = "discover" | "simulator";
export type HealthKind = "ok" | "degraded" | "offline";

export interface AppSettings {
  theme: ThemeMode;
  selectedControllerId: string | null;
  simulatorRunning: boolean;
  wizardCompleted: boolean;
}

export interface ControllerInfo {
  id: string;
  name: string;
  kind: ControllerKind;
  address: string;
  port: number;
  fw: string;
  role: string;
  caps: number;
  lastSeenMs: number;
  online: boolean;
  health: HealthKind;
  latencyMs?: number;
  uptimeMs?: number;
  fps?: number;
}

export interface SnapshotBase {
  controllers: ControllerInfo[];
  selectedId: string | null;
  selected: (ControllerInfo & { status: StatusSnapshot }) | null;
  simulatorRunning: boolean;
  settings: AppSettings;
  lastScanMs: number;
}

export interface Snapshot extends SnapshotBase {
  /** True on first run (no controller chosen yet) — the app shows the wizard. */
  wizardNeeded: boolean;
}

// Result of a config-writing command (rename, Wi-Fi). The firmware replies,
// then reboots to apply — rebootApplied is true whenever the write went
// through, regardless of the reply race. A byte-exact backup is left behind
// before any write so a later phase can restore on failure.
export interface ConfigCommandResult {
  ok: boolean;
  rebootApplied: boolean;
  wifiApplied?: boolean;
  backupPath: string | null;
  detail?: string;
}

export interface AppInfo {
  version: string;
  platform: string;
  isPackaged: boolean;
}

export interface ResonuxApi {
  getInfo(): Promise<AppInfo>;
  getSnapshot(): Promise<Snapshot>;
  setTheme(mode: ThemeMode): Promise<void>;
  selectController(id: string | null): Promise<void>;
  rescan(): Promise<void>;
  setSimulator(enabled: boolean): Promise<void>;
  setBrightness(id: string, value: number): Promise<boolean>;
  setCinematic(id: string, enabled: boolean): Promise<boolean>;
  getStatus(id: string): Promise<StatusSnapshot>;
  getCinematic(id: string): Promise<CinematicPayload>;
  getDevices(id: string): Promise<DevicesPayload>;
  getAudioSources(id: string): Promise<AudioSourcesPayload>;
  renameController(id: string, name: string): Promise<ConfigCommandResult>;
  setWifi(id: string, ssid: string, password: string): Promise<ConfigCommandResult>;
  /** True once the controller answers again (used after a config reboot). */
  waitOnline(id: string, timeoutMs: number): Promise<boolean>;
  finishWizard(): Promise<void>;
  // ---- D3: main control surface ----
  getState(id: string): Promise<StatePayload>;
  getThemes(id: string): Promise<ThemesPayload>;
  selectTheme(id: string, themeId: string, strip?: number): Promise<boolean>;
  setStripEffect(id: string, strip: number, effectId: number): Promise<boolean>;
  selectAudioSource(id: string, source: number): Promise<boolean>;
  setAutoSelect(id: string, autoSelect: boolean): Promise<boolean>;
  triggerCinematicTest(id: string): Promise<boolean>;
  /** D4: run the guided hardware check against a controller's existing REST surface. */
  runHardwareCheck(id: string): Promise<HardwareCheckReport>;
  onChanged(cb: (snapshot: Snapshot) => void): () => void;
}