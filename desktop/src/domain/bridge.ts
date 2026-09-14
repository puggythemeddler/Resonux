// Contract between the Electron main process and the React renderer. The
// renderer talks only to this surface through contextBridge — structured
// data, no Node APIs, no closures.

import type { StatusSnapshot, CinematicPayload, DevicesPayload, AudioSourcesPayload } from "./types";

export type {
  StatusSnapshot,
  CinematicPayload,
  DevicesPayload,
  AudioSourcesPayload,
};

export type ThemeMode = "system" | "dark" | "light";
export type ControllerKind = "discover" | "simulator";
export type HealthKind = "ok" | "degraded" | "offline";

export interface AppSettings {
  theme: ThemeMode;
  selectedControllerId: string | null;
  simulatorRunning: boolean;
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

export interface Snapshot {
  controllers: ControllerInfo[];
  selectedId: string | null;
  selected: (ControllerInfo & { status: StatusSnapshot }) | null;
  simulatorRunning: boolean;
  settings: AppSettings;
  lastScanMs: number;
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
  onChanged(cb: (snapshot: Snapshot) => void): () => void;
}