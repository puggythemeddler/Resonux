# Resonux Control Center — Windows desktop app

A professional Windows desktop application that wraps the existing Resonux
ESP32-S3 platform for **non-technical users**: guided setup, plain language,
no terminal, no JSON editing, no PlatformIO. Everything the firmware and web
dashboard already do — discovery, REST control, OTA, cinematic — is consumed,
not duplicated.

Status: **Phase 1 in the repo** (architecture + client layer + discovery +
simulator + app shell). Buildable and host-tested; cross-checks against real
controllers happen at bench bring-up (`docs/BENCH.md`).

## Why a desktop app at all

The web dashboard is excellent at what a power user wants on a phone or wall
tablet. It assumes you already have the controller on your network and know
what a "resonux controller" is. The desktop app is the other 90% of the
customer: it talks the user through first-run, scans/handshake, firmware
recovery, diagnostics and backups in plain language, and hides the platform
entirely.

## Framework decision

Recorded here with the reasoning, so the choice is auditable.

| Option | Verdict |
|---|---|
| **Electron** (TypeScript + React) | **Chosen** |
| Tauri v2 (Rust + webview) | Not available in this environment — no Rust toolchain or MSVC build tools installed; installer/serial/ble needs would require bootstrapping an entire toolchain. Revisit in a later phase if installer size becomes decisive. |
| .NET / WinUI 3 | Not available — no .NET SDK/workload on this machine. A valid future native path for the installer build if the project ships outside the tooling constraints. |
| PWA / wrapper around `web/` | Rejected: PWA can't own the platform (serial flashing, process supervision, filesystem backup, tray, offline first-run) the way a desktop host can. |

**Why Electron wins for this environment:**

- **Toolchain reality.** Node 24 + npm are present and the repository already
  ships a Vite + React + TypeScript design language. Electron reuses both
  verbatim instead of adding a second compile pipeline.
- **Platform needs map cleanly:** HTTP REST client, UDP multicast discovery
  (Node `dgram`), management of the companion as a child process, filesystem
  backup/restore, native tray, and — in a later phase — `serialport` for pass
  code/flash recovery and `electron-builder` for the installer + auto-update.
- **Security model we want:** a sandboxed renderer with zero Node access, all
  device I/O confined to the main process behind a typed IPC contract
  (`contextIsolation: true`, `sandbox: true`, no `nodeIntegration`).

Costs owned: larger installer footprint (~120 MB unpacked) and Chromium
consumables. These are accepted; the app replaces a web page the user already
loads, not a battery-constrained phone app.

## Architecture

Four layers; **only the client layer talks to devices**.

```
┌────────────────────────────────────────────────────────────┐
│ Windows UI (React renderer, sandboxed, no Node)             │
│   areas: Home · Setup · Lights · Music · Themes ·           │
│          Cinematic · Devices · Diagnostics · Settings       │
│   IPC: window.resonux (contextBridge, typed, event push)    │
├────────────────────────────────────────────────────────────┤
│ Electron main (AppCore)                                     │
│   ControllerRegistry  -> discovery + health poll + snapshot │
│   SettingsStore       -> userData/settings.json             │
│   Simulator           -> MockController (in-process)        │
├────────────────────────────────────────────────────────────┤
│ Resonux Client Layer  (src/client, host-side)               │
│   HttpTransport  (fetch, hard timeout, HttpError)           │
│   UdpDiscovery   (RESO_DISCOVER probe/parse, address learn) │
│   ControllerRegistry (registry + health + offline rules)    │
├────────────────────────────────────────────────────────────┤
│ Hardware services (phases 3–6)                              │
│   REST → firmware WebUi   (all /api/*)                      │
│   companion supervision → tools/companion child process     │
│   OTA / recovery flashing → esptool (later phase)           │
├────────────────────────────────────────────────────────────┤
│ Domain model (src/domain — pure, shared, host-tested)       │
│   wire types · discovery codec · IPC contract               │
└────────────────────────────────────────────────────────────┘
```

### Rules that hold

- **The renderer never touches hardware.** No fetch to devices, no UDP, no
  files. It gets governed snapshots and issues named commands via
  `window.resonux`. This is what keeps a UI bug from bricking a rig.
- **The simulator is honest.** It runs the exact same REST surface as the
  firmware (`MockController`), is always labelled *Simulator* wherever it
  appears, and never reports confirming success for a command it didn't
  actually apply. Its existence is a dev/demo surface — not a fake success
  story. (Applies the review rule from `.impeccable`: the demo must not claim
  the hardware UX is done.)
- **Wire shapes are 1:1.** `src/domain/types.ts` mirrors the firmware
  `WebUi` JSON exactly; `src/domain/discovery.ts` is a byte-compatible port of
  `firmware/src/device/DiscoverProtocol.h` (same probe, prefix, key order,
  space-sanitizing). Divergence is a host-test failure, not a documentation
  footnote.
- **Catalogs come from the device.** Capability/connection catalogs are read
  from `/api/devices` (`capCatalog`/`connCatalog`), exactly like the web
  dashboard; the desktop app never hardcodes a capability id.
- **A controller that stops answering stops pretending.** Offline chips show
  last-known identity (name, address) but stale stats are cleared rather than
  shown beside a red dot.
- **No silent destructive path.** Every reboot/restore/reset sits behind an
  explicit confirm or an explicit reason (the web review's P0/P1 findings are
  carried as a checklist, not drifted back in).
- **No secrets in files or logs.** `settings.json` holds theme/selection only.
  Diagnostics exports redact IP hostnames by default.

Right now (Phase 1) the "layers above device" boundary is already real and
host-tested; the app runs and controls either a discovered controller or the
simulator.

## What the desktop app consumes (audit result)

From `firmware/src/web/WebUi.cpp` (full report with line refs):

| Endpoint | Firmware | Phase |
|---|---|---|
| `GET /api/status` | `sendStatus()` | 1 (health rail + home stats) |
| `GET /api/frame` | `sendFrame()` | 1 (home FPS) |
| `GET /api/state` | `sendState()` | 1 (brightness source of truth) |
| `GET/POST /api/cinematic`, `POST /api/cinematic/test` | `sendCinematic()`/`handleCinematicPut()`/`handleCinematicTest()` | 1 (home toggle) / 5 (full) |
| `GET /api/devices`, `POST /api/devices/scan`, device CRUD | `sendDevices()`/`handleDevicesScan()`/`handleDevice()` | 1 (inputs summary) / 4 |
| `GET /api/audio/sources`, `POST /api/audio/source` | `sendAudioSources()`/`handleAudioSource()` | 2 (music) |
| `GET/PUT /api/config`, theme CRUD (`/api/themes*`) | `WebUi.cpp:243`+ | 2 (setup) / 3 (themes) |
| `POST /api/state/brightness`, `/effect`, `/sensitivity`, `/backlight`, `/timeout` | live endpoints — **no reboot** | 1+ |
| `POST /api/system/restart`, `/power-off`; `GET /api/system/status` | `WebUi.cpp:315+` | 4 (diagnostics) |
| `POST /api/ota` | `WebUi.cpp:58` | 6 (firmware) |
| ArduinoOTA | `ArduinoOTA.begin()` | 6 |

Discovery: `RESO_DISCOVER` UDP multicast `239.255.42.10:9770`, probe
`RESO_DISCOVER v1\n`, reply prefix `RESO-DISCOVER-RESP` with
`id/name/kind/caps/source/fw/role` — **no IP in the reply**; the desktop app
learns the controller's address from the UDP source address of the response
(`src/client/discovery.ts`).

Companion: `tools/companion/resonux_companion.py` is managed as a child
process by the desktop app in a later phase — the user never runs Python.

## Domain model

- `src/domain/types.ts` — firmware wire types (1:1 as above).
- `src/domain/discovery.ts` — RESO_DISCOVER codec port (+ `buildDiscoverResponse`
  for the simulator/tests).
- `src/domain/bridge.ts` — `ResonuxApi`, the IPC contract; `Snapshot`,
  `ControllerInfo`, `AppSettings`, health states.
- `src/domain/catalog.ts` — local default labels only (device catalogs win).

## Client layer (Phase 1)

- `src/client/http.ts` — typed JSON transport, hard timeout, `HttpError`
  (`status` + raw body) so the UI can always explain *what happened, why, what
  to do*, not "fetch failed".
- `src/client/discovery.ts` — UDP probe/responder listener on every non-internal
  IPv4 interface; `ping()` for manual rescans.
- `src/client/registry.ts` — controller registry: last-seen, health poll,
  offline-after-2-failures, latency via `performance.now()` around the status
  call, charged snapshot assembly, coalesced change events.

## Simulator (Phase 1)

`src/sim/mock.ts` — `MockController`:

- Serves firmware-identical JSON for `/api/status`, `/api/state`, `/api/frame`,
  `/api/cinematic` (+POST toggle), `/api/cinematic` state, `/api/devices`,
  `/api/audio/sources`, `/api/config`, `/api/system/status`,
  `/api/state/brightness` (POST, validates 0–255).
- Deterministic drift (no `Math.random()`), honest labels
  (`device: "Simulator / Demo"`), loopback-bound `127.0.0.1:<port>`.
- Registered with the registry as `kind: "simulator"` so the UI always shows
  it for what it is.

## Testing (host, no hardware)

`desktop/test` runs via vitest; dev command `npm test` in `desktop/`.

| Suite | Covers |
|---|---|
| `src/domain/discovery.test.ts` | codec round-trip, lenient unknown-key parse, non-responder rejection — keeps the TS port byte-compatible with the C++ codec |
| `src/sim/mock.test.ts` | REST parity: status shape, brightness mutation + 0–255 validation, cinematic toggle, 404s |
| `src/client/registry.test.ts` | simulator health → online; going offline clears stale status (identity survives); removal |
| `src/core/app.test.ts` | full AppCore boot → auto simulator → select → live commands round-trip; simulator stop→drop |

Repository-wide, `pio test -e native` remains the firmware gate (144 tests)
and `web` runs `tsc --noEmit` + `npm run build`. The desktop app adds its own
verification lane: `npm run typecheck`, `npm test`, `npm run build`.

## Verification commands (desktop)

```bash
cd desktop
npm install
npm run typecheck      # tsc main + renderer
npm test               # vitest (host, no hardware)
npm run build          # tsc main + vite renderer → dist/
npm start              # build + launch Electron
```

Automated boot smoke test (no visible window, exits non-zero on failure):

```powershell
cd desktop
npm run build:main
$env:RESONUX_SMOKE="1"; npx electron .; Remove-Item Env:RESONUX_SMOKE
```

## Phase plan

| Phase | Scope | Status |
|---|---|---|
| **1** | Architecture + client layer + discovery + simulator + app shell (Home/Setup) | **in repo, builds + tests green** |
| **2** | First-run setup wizard (highest priority): guided connect, verify, rename, Wi-Fi handoff, honest errors | planned |
| **3** | Main Control Center: Lights (per strip, by name), Music (sources, plain language), Themes (swatches) | planned |
| **4** | Diagnostics: health, logs, structured reports (no secrets), restart/power-off with confirms | planned |
| **5** | Cinematic: scenes, moods, comfort, QA test bursts, companion status | planned |
| **6** | Firmware & recovery: OTA update, backup/restore, flash recovery via bundled esptool | planned |
| **7** | Polish: installer (electron-builder), auto-update, tray, UX audit vs `.impeccable` review standards | planned |

Rules for the plan: every phase changes README + the docs that describe it in
the same commit; destructive capabilities (restore, reset, flash) get explicit
confirmation UX; the simulator is updated in lockstep so no screen is
un-demonstrable.

## Design standards (from the repo's review history)

The `.impeccable/` critiques of the web dashboard are carried forward as the
desktop app's bar, not rewritten:

- Instrument vocabulary for a lighting system, not IDE/JSON vocabulary
  (no "Sensitivity 0.50" next to "value: <int>").
- One slider primitive that knows its unit; brightness is **percent**, mapped
  to the firmware 0–255 state endpoint at the boundary.
- Live tuning hits live `/api/state*` endpoints only — never a config PUT
  (the web app's P0 was the full-config PUT that rebooted every 3 s).
- Destructive actions: explicit confirm; no Reset next to a "New" button.
- Token discipline: two palettes (dark/light) expressed as variables; no
  brand-bleed hex values floating in components.
- Inline help where a first-timer will meet a term ("theme", "companion",
  "conditioning").
- Demo/simulator honesty: clearly labelled, never substitutes for hardware
  QA claims.