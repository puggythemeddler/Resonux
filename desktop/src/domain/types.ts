// Wire shapes as emitted by the firmware WebUi (firmware/src/web/WebUi.cpp)
// and config store. Kept 1:1 with the ESP32 REST surface so the desktop
// client layer and simulator stay byte-compatible with the controller.

export interface WifiStatus {
  mode: string;
  ip: string;
  connected: boolean;
}

export interface SyncStatus {
  enabled: boolean;
  role: string; // "off" | "master" | "slave"
  seq?: number;
  offsetMs?: number;
  group?: string;
  port?: number;
  masterAlive?: boolean;
}

export interface ArtNetStatus {
  enabled: boolean;
  fixtures: number;
  status: string;
}

export interface SystemStatus {
  state: string;
  action: string;
}

export interface DisplayStatus {
  enabled: boolean;
  backlightPct: number;
  timeoutS: number;
  awake: boolean;
  wakePin: number;
}

export interface StatusSnapshot {
  ok: boolean;
  device: string;
  uptimeMs: number;
  heap: number;
  fps: number;
  stripCount: number;
  wifi: WifiStatus;
  sync: SyncStatus;
  artnet: ArtNetStatus;
  system: SystemStatus;
  display: DisplayStatus;
}

export interface FrameSample {
  ok: boolean;
  fps: number;
  amp: number;
  bass: number;
  lowMid: number;
  mid: number;
  highMid: number;
  treble: number;
  beat: boolean;
  beatStrength: number;
  bandCount: number;
  bands: number[];
  peaks: number[];
}

export interface StatePayload {
  theme: { global: string; strips: string[] };
  effect: number[];
  brightness: number;
  stripCount: number;
  themeCount: number;
}

export interface CinematicConfigWire {
  enabled: boolean;
  mode: number;
  modeId: string;
  modeLabel: string;
  genre: number;
  genreId: string;
  genreLabel: string;
  comfort: number;
  comfortId: string;
  comfortLabel: string;
  syncOffsetMs: number;
  sensitivity: number;
  reaction: number;
  visualInfluence: number;
  audioInfluence: number;
  colorInfluence: number;
  speed: number;
  smoothing: number;
  flashIntensity: number;
  flashDurationMs: number;
  flashMinGapMs: number;
  boomCooldownMs: number;
  whisperDim: number;
  maxBrightness: number;
  ambientFloor: number;
  roomMapping: boolean;
  waveSpeed: number;
  waveDecay: number;
  waveWidth: number;
  maxWaves: number;
  receiveUdp: boolean;
  group: string;
  port: number;
  staleMs: number;
  demo: boolean;
}

export interface CinematicStatusWire {
  source: number;
  scene: number;
  sceneId: string;
  event: number;
  eventId: string;
  eventConfidence: number;
  sceneConfidence: number;
  luminance: number;
  motion: number;
  progAudio: number;
  hue: number;
  sat: number;
  val: number;
  audioLevel: number;
  boom: boolean;
  tension: number;
  lastFrameMs: number;
  mood: number;
  moodId: string;
  moodLabel: string;
  moodEnergy: number;
  recentEvents: number;
  spatialActive: boolean;
  linkLatencyMs: number;
  jitterMs: number;
}

export interface CinematicPayload {
  config: CinematicConfigWire;
  active: boolean;
  companionAlive: boolean;
  companionSource: string;
  companionSourceCount: number;
  companionSkewPpm: number;
  companionLastRxMs: number;
  demoActive: boolean;
  status: CinematicStatusWire;
}

export interface DeviceConfidence {
  identity: number;
  capability: number;
  integration: number;
}

export interface DeviceIntegration {
  kind: string;
  title: string;
  route: string;
  safe: boolean;
}

export interface DeviceRow {
  id: string;
  name: string;
  profileId: string;
  capabilities: number;
  connection: string;
  connectionLabel: string;
  status: string;
  statusLabel: string;
  source: number;
  persisted: boolean;
  needsConditioning: boolean;
  note: string;
  lastSeen: number;
  fw?: string;
  role?: string;
  caps?: string[];
  protocols?: string[];
  safetyNotes?: string[];
  confidence?: DeviceConfidence;
  integrations?: DeviceIntegration[];
}

export interface BuiltinProfile {
  id: string;
  name: string;
  manufacturer: string;
  deviceType: string;
  capabilities: number;
  needsConditioning: boolean;
}

export interface CatalogEntry {
  id: string;
  label: string;
}

export interface DevicesPayload {
  ok: boolean;
  devices: DeviceRow[];
  profiles: BuiltinProfile[];
  capCatalog: CatalogEntry[];
  connCatalog: CatalogEntry[];
  found?: number;
  registered?: number;
}

export interface AudioSourceOption {
  id: number;
  ident: string;
  label: string;
  available: boolean;
  active: boolean;
}

export interface AudioSourcesPayload {
  ok: boolean;
  active: number;
  reason: string;
  current: number;
  autoSelect: boolean;
  preferred: number;
  fallback: number;
  sources: AudioSourceOption[];
}

// The full config is a free-form document (ConfigStore dump). Typed surface
// only where the desktop app currently reads it.
export interface ConfigPayload {
  deviceName: string;
  stripCount: number;
  masterBrightness: number;
  net: {
    enabled: boolean;
    mode: number;
    apSsid: string;
    apPassword: string;
    staSsid: string;
    staPassword: string;
  };
  [key: string]: unknown;
}

// RESO_DISCOVER goes through firmware discovery (seen across an entire scan
// cycle) rather than the device REST catalog, so these are local defaults
// only — the authoritative capability/connection catalogs come from
// /api/devices.
export const DEFAULT_CONNECTION_LABELS: Record<string, string> = {
  aux_in: "Auxiliary input",
  bluetooth: "Bluetooth",
  dmx: "DMX",
  artnet: "Art-Net",
  network: "Network",
  usb: "USB",
  sync_udp: "Resonux Sync (multicast)",
  scene_udp: "Cinematic companion (multicast)",
};

export const WIFI_MODE_LABELS: Record<string, string> = {
  ap: "Access point",
  sta: "Wi-Fi",
  ap_sta: "Wi-Fi + hotspot",
  off: "Off",
};

export const SYNC_ROLE_LABELS: Record<string, string> = {
  off: "Sync off",
  master: "Sync master",
  slave: "Sync slave",
};

export const HEALTH_LABELS: Record<string, string> = {
  ok: "Healthy",
  degraded: "Degraded",
  offline: "Offline",
};