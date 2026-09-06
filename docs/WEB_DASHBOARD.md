# Web Dashboard Architecture — TypeScript + React

Served locally by the ESP32. No cloud account, no external CDN, works fully
offline on the controller's own Wi-Fi AP.

## 1. Stack & why

| Layer | Choice | Why |
|---|---|---|
| Language | **TypeScript** | typed config ↔ API contract shared with firmware |
| Framework | **React** | component re-use for Strips/Zones/Presets; fine on small devices |
| Build | **Vite** | fast dev, small tree-shaken bundles, easy embed target |
| Routing | react-router (hash) | no server rewrites needed behind a static file server |
| State | lightweight hooks + context | avoid heavy stores; each page owns its config slice |
| HTTP/stream | fetch (REST) + **SSE** | EventSource is built into browsers; no WS lib needed |
| UI kit | none / minimal hand-rolled CSS | keep bundle lean; mobile-first responsive |

Target bundle: **≤ 300 kB gzip** (React ≈ 45 kB gzip; TS + our lib negligible),
so it embeds into LittleFS and loads instantly on the AP network.

## 2. Deployment topologies

```
   dev:  Vite dev server ──┐
                           ├──► browser   (proxy /api + /stream → ESP32 IP:80)
   prod: npm run build ──► firmware/data/web.zip ──► LittleFS
         ESP32 serves static assets + REST + SSE on :80
```

- Firmware exposes exactly one origin (`http://<esp-ip>/`). Dev uses a Vite
  proxy (`/api`, `/stream`) to that origin.
- `web/` has `src/services/config.ts`, `src/services/live.ts`, `src/api/types.ts`
  — types mirror the firmware JSON schema (single source of truth documented in
  `docs/ARCHITECTURE.md §13/§19`).

## 3. Firmware API surface (tentative, Phase 4)

| Method & path | Purpose |
|---|---|
| `GET /api/config` | full config document |
| `PUT /api/config` | validate + replace (reboot-flag if deep) |
| `GET /api/strips/{i}` `PUT /api/strips/{i}` | per-strip config slice |
| `GET /api/presets` `POST/DELETE /api/presets/{name}` | preset CRUD + apply |
| `POST /api/effect/{strip}/{id}` `PUT /api/effect/{strip}/params` | live effect switching / params |
| `POST /api/test` | LED test mode (single shot / cycle) |
| `GET /api/diag` | mem/heap, cpu@tasks, uptime, temperatures, config ver, errors |
| `POST /api/reboot`, `GET /api/info` | system |
| `GET /stream` | SSE: `AudioFrame` (10–20 Hz) + strip preview frames (lower rate) + status |
| `POST /api/ota` (Ph 8) | OTA upload |

- Admin token: `X-Admin` header on all state-changing endpoints.
- All numeric/enum inputs validated on firmware (range + pin whitelist) before
  commit.

## 4. Dashboard sections (spec §21)

| Route | Contents |
|---|---|
| `/` Dashboard | live spectrum bars, bass/mid/treble meters, amplitude, beat lamp + strength, per-strip preview, quick bright/sens |
| `/led` LED Setup | add/remove strips, LED type, count, pins, protocol, colour order, brightness, power limits, test buttons |
| `/audio` Audio | input mode, mic pin/gain, FFT size, db floor/ceil, gate, attack/release, sensitivity meters |
| `/effects` Effects | per strip: effect, params (speed = hueSpeed, sensitivity, decay, brightness range), palette picker |
| `/mapping` Frequency Mapping | per strip: zone/ranges → band; colour-per-band editor |
| `/strips` Strips | overview grid, per-strip config (mirrors this spec's zones), previews |
| `/presets` Presets | save/load/duplicate/delete; gallery (Party, Bass Heavy, Spectrum, Rainbow, Club, Chill, Rock, EDM, Vocal, Beat, Ambient, Custom) |
| `/power` Power | PSU sizing helper, per-strip current estimate, limits |
| `/system` System | device name, Wi-Fi mode/AP config, OTA, reboot, firmware info |
| `/diagnostics` Diagnostics | mic detect, audio level, FFT ok, LED-driver ok, memory, CPU, config version, errors (spec §31) |

## 5. Live data & preview

- SSE pushes `{t, bands, amp, bass, lowMid, mid, highMid, treble, beat,
  beatStrength, fps, frame?}`.
- `frame` (when preview on): compact run-length-encoded strip RGB at reduced
  rate, canvas-rendered with strip geometry (per-strip, per-segment).
- Preview is *true* (firmware actually computed it) or, in the standalone
  desktop dev tool, the Python file simulator substitutes the same schema.

No cloud, no auth wall for viewing; writes require admin token (§21).

## 6. Security notes (spec §33)

- AP mode WPA2, random default SSID/pass printed to serial.
- X-Admin token; linear-time token compare; no secrets in static files; CORS
  locked to same-origin; input validation on both sides; OTA guarded.
- Firmware disconnect/stale states: UI shows "controller offline" gracefully.