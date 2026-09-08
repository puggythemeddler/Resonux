# Web Dashboard Architecture — TypeScript + React

Status: **implemented in `web/`** — Vite + React + TS SPA, embedded into
`firmware/data/web/` and served by `src/web/WebUi` on the device. Live,
Themes, Configuration, and Firmware Update tabs are working. Uses short-interval
`fetch` polling of `/api/frame` for the live spectrum (SSE remains a future
optimization).

Served locally by the ESP32. No cloud account, no external CDN, works fully
offline on the controller's own Wi-Fi AP.

`npm run dev` runs a **simulator**: a Vite dev-middleware mock
(`web/mockDevPlugin.ts`) serves `*/api/status`, `*/api/frame`, `*/api/config`
(GET/PUT), `*/api/themes` (GET/PUT/DELETE + `select`/`reset`), `*/api/state`
(+ `brightness`/`effect`), `*/api/reboot` and `*/api/ota`, so the whole UI can
be developed and demoed with no hardware.

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
   dev:  Vite dev server + mock /api  ──► browser (simulator, no hardware)
   prod: npm run build ──► firmware/data/web/ ──► SPIFFS (uploadfs)
         ESP32 serves static assets + REST on :80
```

- Firmware exposes exactly one origin (`http://<esp-ip>/`).
- Build outputs to `firmware/data/web` so `pio run -t uploadfs` flashes it
  to the SPIFFS partition alongside `config.json`.

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
| `GET /api/themes` | list themes `{themes:[ThemeDef], active}` |
| `PUT /api/themes` | upsert a theme (`id` required, builtin ids are validated) |
| `DELETE /api/themes?id=…` | delete a non-builtin theme |
| `POST /api/themes/select` `{id}` / `{id,strip}` | apply theme globally or per strip |
| `POST /api/themes/reset` | reinstall the default built-in themes |
| `GET /api/state` | live controller state (active theme, per-strip themes/effects, master brightness) |
| `POST /api/state/brightness` `{value}` | live master LED brightness (no reboot) |
| `POST /api/state/effect` `{strip,id}` | live per-strip effect override |

`ThemeDef` wire shape (`ThemeEngine::encode`/`decode`): `id`, `name`,
`builtin`, `brightness` (`{base,min}`), `saturation`, `palette` (`#RRGGBB`
hex strings), `description`, `response` (`{bass,lowMid,mid,highMid,treble,
beat,amp}`), `animation` (`{flash,contrast,density,movement,pulse,sparkle,
smoothing}`), `effects` (preferred `Effect` ids — manual per-strip overrides
always win, these are hints only). The dashboard's **PersonaBar** on each card
condenses the response curve into Bass/Groove/Treble/Movement; the Live tab
pushes master brightness through `/api/state/brightness` (poll-synced, no LED
flicker).

- Admin token: `X-Admin` header on all state-changing endpoints.
- All numeric/enum inputs validated on firmware (range + pin whitelist) before
  commit. `ConfigStore::load` clamps every numeric field (ints and floats) to
  sane bounds, rejects non-finite floats, bounds string fields, and returns
  `false` for configs claiming a *newer* schema version — so a downgraded
  firmware never clobbers a newer config.

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
| `/themes` Themes | implemented: palette swatches + brightness/saturation, response + animation curves; select (global/per-strip), create/edit/delete non-built-ins, reset defaults |
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