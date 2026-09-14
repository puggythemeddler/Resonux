// Contract between the Electron main process and the React renderer. The
// renderer talks only to this surface through contextBridge — structured
// data, no Node APIs, no closures.

import type { StatusSnapshot, CinematicPayload, DevicesPayload, AudioSourcesPayload, ThemesPayload, StatePayload, AudioSourceOption, ThemeDefWire, HardwareCheckReport, CheckStepResult, CheckVerdict } from "./types";
import type { LogEvent, LogLevel } from "../log/log";

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
  LogEvent,
  LogLevel,
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
  /** Recent session events (in-memory only, nothing secret, cap ~200). */
  log: LogEvent[];
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

/** Outcome of a system command (restart / power-off) on a controller. */
export interface SystemCommandResult {
  ok: boolean;
  action: "restart" | "power_off";
  detail?: string;
}

/** Outcome of "save this report to a file" (the dialog lives in the main process). */
export interface ExportReportResult {
  saved: boolean;
  filePath?: string;
  detail?: string;
}

/** Outcome of a firmware OTA upload to a controller. */
export interface UpdateFirmwareResult {
  ok: boolean;
  detail?: string;
}

/** Outcome of "save the current config to a file". */
export interface ConfigBackupResult {
  saved: boolean;
  filePath?: string;
  detail?: string;
}

/** Outcome of "restore a previously saved config to a controller". */
export interface ConfigRestoreResult {
  ok: boolean;
  rebootApplied: boolean;
  detail?: string;
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
  // ---- D4: system controls + session log + report export ----
  /** Gracefully restart the controller (confirms first in the UI). */
  restartController(id: string): Promise<SystemCommandResult>;
  /** Gracefully power the controller off into deep sleep (confirmed first in the UI). */
  powerOff(id: string): Promise<SystemCommandResult>;
  /** Export a structured report (facts only, no secrets) to a file the user picks. */
  exportReport(id: string, check: HardwareCheckReport | null): Promise<ExportReportResult>;
  // ---- D5: firmware update + config backup/restore ----
  /** Pick a firmware .bin in the main process and OTA-upload it to the controller. */
  updateFirmware(id: string): Promise<UpdateFirmwareResult>;
  /** Pick a save location in the main process and write the live config there. */
  backupConfig(id: string): Promise<ConfigBackupResult>;
  /** Pick a saved config .json in the main process and PUT it to the controller. */
  restoreConfig(id: string): Promise<ConfigRestoreResult>;
  onChanged(cb: (snapshot: Snapshot) => void): () => void;
}