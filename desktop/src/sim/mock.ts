// MockController — an in-process pseudo-controller that serves the same REST
// surface as the firmware WebUi. It exists so the desktop app has something
// real to talk to during development, demos, and first-run setup on machines
// with no controller on the network.
//
// It is always labelled as a simulator wherever the UI shows it; it never
// pretends to be a physical device. State is deliberately small and honest:
// every value an endpoint returns is the value the matching mutation wrote.

import http from "node:http";
import type { StatusSnapshot, CinematicPayload, AudioSourcesPayload } from "../domain/types";
import type { DevicesPayload } from "../domain/types";

export interface MockControllerOptions {
  name?: string;
  id?: string;
  apMode?: boolean;
}

function clamp01(v: number): number {
  return Math.max(0, Math.min(1, v));
}

export class MockController {
  readonly id: string;
  private _name: string;
  private _apMode: boolean;
  private server: http.Server | null = null;
  private port = 0;
  private ticks = 0;
  private netSsid = "";
  private netPassword = "";
  private netMode = 1; // 0 = ap only, 1 = sta

  private status: StatusSnapshot = {
    ok: true,
    device: "Simulator / Demo",
    uptimeMs: 0,
    heap: 215 * 1024,
    fps: 60,
    stripCount: 2,
    wifi: { mode: "sta", ip: "127.0.0.1", connected: true },
    sync: { enabled: true, role: "master", seq: 0, offsetMs: 0, group: "239.255.42.9", port: 9769 },
    artnet: { enabled: false, fixtures: 0, status: "off" },
    system: { state: "reactive", action: "idle" },
    display: { enabled: true, backlightPct: 100, timeoutS: 300, awake: true, wakePin: -1 },
  };

  private brightness = 255;
  private cinematicActive = true;

  constructor(opts: MockControllerOptions = {}) {
    this.id = opts.id ?? "sim-demo";
    this._name = opts.name ?? "Demo Room";
    this._apMode = Boolean(opts.apMode);
    if (this._apMode) {
      this.status.wifi = { mode: "ap", ip: "192.168.4.1", connected: false };
      this.netMode = 0;
    }
  }

  get name(): string {
    return this._name;
  }

  get apMode(): boolean {
    return this._apMode;
  }

  listen(): Promise<number> {
    return new Promise((resolve, reject) => {
      this.server = http.createServer((req, res) => this.route(req, res));
      this.server.on("error", reject);
      this.server.listen(0, "127.0.0.1", () => {
        const addr = this.server?.address();
        const listenPort = typeof addr === "object" && addr ? addr.port : 0;
        this.port = listenPort;
        resolve(listenPort);
      });
    });
  }

  get portNumber(): number {
    return this.port;
  }

  close(): Promise<void> {
    return new Promise((resolve) => {
      if (!this.server) return resolve();
      this.server.close(() => resolve());
      this.server = null;
    });
  }

  // Closes and re-listens on the SAME loopback port, keeping state. Lets the
  // reconnect path be exercised honestly: address and id stay stable while the
  // server itself is briefly gone — like a controller restart after a config
  // write, minus the physical reboot.
  async restart(): Promise<void> {
    await this.close();
    await new Promise<void>((resolve, reject) => {
      this.server = http.createServer((req, res) => this.route(req, res));
      this.server.on("error", reject);
      this.server.listen(this.port, "127.0.0.1", () => {
        this.ticks = 0;
        resolve();
      });
    });
  }

  now(): { ticks: number; fps: number; beat: boolean; bass: number; mid: number; amp: number } {
    this.ticks += 1;
    const fps = 60 + Math.round(Math.sin(this.ticks / 30) * 6);
    const beat = this.ticks % 67 === 0;
    return {
      ticks: this.ticks,
      fps,
      beat,
      bass: clamp01(Math.sin(this.ticks / 11) * 0.5 + 0.5),
      mid: clamp01(Math.sin(this.ticks / 7 + 1) * 0.5 + 0.5),
      amp: clamp01(Math.sin(this.ticks / 17) * 0.35 + 0.5),
    };
  }

  private route(req: http.IncomingMessage, res: http.ServerResponse): void {
    const url = (req.url ?? "/").split("?")[0];
    const method = req.method ?? "GET";
    const json = (code: number, body: unknown) => {
      const text = JSON.stringify(body);
      res.writeHead(code, {
        "content-type": "application/json",
        "content-length": Buffer.byteLength(text),
      });
      res.end(text);
    };

    const readBody = (): Promise<Record<string, unknown>> =>
      new Promise((resolve) => {
        let data = "";
        req.on("data", (chunk) => (data += chunk));
        req.on("end", () => {
          try {
            resolve(data ? (JSON.parse(data) as Record<string, unknown>) : {});
          } catch {
            resolve({});
          }
        });
      });

    switch (`${method} ${url}`) {
      case "GET /api/status": {
        this.status.uptimeMs = this.ticks * 100;
        this.status.fps = this.now().fps;
        this.status.sync.seq = this.ticks;
        json(200, this.status);
        return;
      }
      case "GET /api/state": {
        json(200, {
          theme: { global: "classic", strips: ["classic"] },
          effect: [1],
          brightness: this.brightness,
          stripCount: this.status.stripCount,
          themeCount: 7,
        });
        return;
      }
      case "GET /api/frame": {
        const n = this.now();
        const bars = [0.2, 0.55, n.bass, 0.4, 0.7, n.mid];
        json(200, {
          ok: true,
          fps: n.fps,
          amp: n.amp,
          bass: n.bass,
          lowMid: n.mid * 0.7,
          mid: n.mid,
          highMid: 0.3,
          treble: 0.25,
          beat: n.beat,
          beatStrength: n.beat ? 1 : 0,
          bandCount: bars.length,
          bands: bars,
          peaks: bars.map((v) => v + 0.15),
        });
        return;
      }
      case "GET /api/cinematic": {
        json(200, this.cinematicPayload());
        return;
      }
      case "POST /api/cinematic": {
        void readBody().then((b) => {
          if (typeof b.enabled === "boolean") this.cinematicActive = b.enabled;
          json(200, { ok: true });
        });
        return;
      }
      case "POST /api/state/brightness": {
        void readBody().then((b) => {
          const v = typeof b.value === "number" ? Math.round(b.value) : -1;
          if (v < 0 || v > 255) {
            json(400, { ok: false, error: "value 0..255" });
            return;
          }
          this.brightness = v;
          json(200, { ok: true });
        });
        return;
      }
      case "GET /api/devices": {
        json(200, this.devicesPayload());
        return;
      }
      case "GET /api/audio/sources": {
        json(200, this.audioSourcesPayload());
        return;
      }
      case "GET /api/config": {
        json(200, {
          deviceName: this.name,
          stripCount: this.status.stripCount,
          masterBrightness: this.brightness,
          net: {
            enabled: true,
            mode: this.netMode,
            apSsid: "Resonux",
            apPassword: "",
            staSsid: this.netSsid,
            staPassword: this.netPassword,
          },
        });
        return;
      }
      case "PUT /api/config": {
        void readBody().then((b) => {
          if (typeof b.deviceName === "string" && b.deviceName.trim()) {
            this._name = b.deviceName.trim();
            this.status.device = this._name;
          }
          const net = (b.net as Record<string, unknown> | undefined) ?? {};
          if (typeof net.staSsid === "string") {
            this.netSsid = net.staSsid;
            this.netPassword = typeof net.staPassword === "string" ? net.staPassword : "";
            if (this.netSsid && this.status.wifi.mode === "ap") {
              // Simulates the Wi-Fi hand-off: joins the given network.
              this.status.wifi = { mode: "sta", ip: "192.168.4.100", connected: true };
              this.netMode = 1;
            }
          }
          // A real controller replies and then reboots to apply. The simulator
          // applies instantly — an honest difference we never hide.
          json(200, { ok: true, rebooting: false });
        });
        return;
      }
      case "GET /api/system/status": {
        json(200, { ok: true, state: this.status.system.state, action: this.status.system.action });
        return;
      }
      default:
        json(404, { ok: false, error: "not_found" });
    }
  }

  private cinematicPayload(): CinematicPayload {
    const n = this.now();
    const active = this.cinematicActive;
    return {
      config: {
        enabled: true,
        mode: 0,
        modeId: "companion",
        modeLabel: "Companion",
        genre: 1,
        genreId: "action",
        genreLabel: "Action",
        comfort: 2,
        comfortId: "concert",
        comfortLabel: "Concert",
        syncOffsetMs: 0,
        sensitivity: 0.5,
        reaction: 1.0,
        visualInfluence: 0.6,
        audioInfluence: 0.4,
        colorInfluence: 1.0,
        speed: 1.0,
        smoothing: 0.5,
        flashIntensity: 0.8,
        flashDurationMs: 120,
        flashMinGapMs: 90,
        boomCooldownMs: 160,
        whisperDim: 0.35,
        maxBrightness: 255,
        ambientFloor: 4,
        roomMapping: false,
        waveSpeed: 1.0,
        waveDecay: 0.4,
        waveWidth: 0.3,
        maxWaves: 3,
        receiveUdp: true,
        group: "239.255.42.11",
        port: 9772,
        staleMs: 1500,
        demo: false,
      },
      active,
      companionAlive: active,
      companionSource: active ? "primary" : "",
      companionSourceCount: active ? 1 : 0,
      companionSkewPpm: 0,
      companionLastRxMs: active ? 40 : 0,
      demoActive: false,
      status: {
        source: active ? 0 : -1,
        scene: active ? 3 : -1,
        sceneId: active ? "action" : "off",
        event: active ? 1 : -1,
        eventId: active ? "impact" : "off",
        eventConfidence: 0.8,
        sceneConfidence: 0.7,
        luminance: n.bass,
        motion: n.mid,
        progAudio: n.amp,
        hue: 22,
        sat: 240,
        val: 230,
        audioLevel: n.amp,
        boom: n.beat,
        tension: 0.5 + Math.sin(this.ticks / 9) * 0.2,
        lastFrameMs: 32,
        mood: active ? 2 : -1,
        moodId: active ? "driving" : "off",
        moodLabel: active ? "Driving" : "Off",
        moodEnergy: 0.65,
        recentEvents: 3,
        spatialActive: false,
        linkLatencyMs: 3,
        jitterMs: 1,
      },
    };
  }

  private devicesPayload(): DevicesPayload {
    return {
      ok: true,
      devices: [
        {
          id: "sim-device-mic",
          name: "Built-in microphone",
          profileId: "mic",
          capabilities: 1,
          connection: "usb",
          connectionLabel: "USB",
          status: "ready",
          statusLabel: "Ready",
          source: 0,
          persisted: true,
          needsConditioning: false,
          note: "Simulated input",
          lastSeen: 0,
        },
      ],
      profiles: [],
      capCatalog: [
        { id: "microphone", label: "Microphone input" },
        { id: "aux", label: "Aux input" },
        { id: "artnet", label: "Art-Net output" },
        { id: "sync", label: "Sync multicast" },
        { id: "scene", label: "Scene multicast" },
      ],
      connCatalog: [
        { id: "usb", label: "USB" },
        { id: "network", label: "Network" },
      ],
    };
  }

  private audioSourcesPayload(): AudioSourcesPayload {
    return {
      ok: true,
      active: 0,
      reason: "ok",
      current: 0,
      autoSelect: false,
      preferred: 0,
      fallback: 2,
      sources: [
        { id: 0, ident: "mic", label: "Microphone", available: true, active: true },
        { id: 1, ident: "aux", label: "Aux input", available: false, active: false },
        { id: 2, ident: "test", label: "Test tone", available: true, active: false },
        { id: 3, ident: "network", label: "Network", available: false, active: false },
      ],
    };
  }
}