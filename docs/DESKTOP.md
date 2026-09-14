# Resonux Control Center — Windows desktop app

A professional Windows desktop application that wraps the existing Resonux
ESP32-S3 platform for **non-technical users**: guided setup, plain language,
no terminal, no JSON editing, no PlatformIO. Everything the firmware and web
dashboard already do — discovery, REST control, OTA, cinematic — is consumed,
not duplicated.

Status: **Phases 1–4 in the repo** (architecture + client layer + discovery +
simulator + app shell + first-run wizard + the main Control Center surface +
a guided hardware check in Diagnostics). Buildable and host-tested;
cross-checks against real controllers happen at bench bring-up
(`docs/BENCH.md`).

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
| `GET/PUT /api/config` | `WebUi.cpp:243`+ | 2 (setup) |
| `GET/PUT/DELETE /api/themes`, `POST /api/themes/select|reset` | `ThemeEngine` encode/apply | 3 (themes) |
| `POST /api/state/effect` | live endpoint — **no reboot** | 3 (lights per strip) |
| `POST /api/state/brightness`, `/sensitivity`, `/backlight`, `/timeout` | live endpoints — **no reboot** | 1+ |
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

- `src/domain/types.ts` — firmware wire types (1:1 as above) + local default
  labels where a catalog doesn't exist yet.
- `src/domain/discovery.ts` — RESO_DISCOVER codec port (+ `buildDiscoverResponse`
  for the simulator/tests).
- `src/domain/bridge.ts` — `ResonuxApi`, the IPC contract; `Snapshot`,
  `ControllerInfo`, `AppSettings`, health states, `ConfigCommandResult`.

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
- Phase 3 additions: `/api/themes` (GET, PUT adds/updates, DELETE non-built-in
  with `?id=`, `POST /api/themes/select` global or per-strip, `POST
  /api/themes/reset` back to built-ins), `POST /api/state/effect` (per-strip,
  validates strip + effect id), `POST /api/audio/source` (source + auto-select
  toggle reflected by `/api/audio/sources`), `POST /api/cinematic/test` (QA
  burst), and `/api/state` reporting the live theme/effect/brightness state.
- Deterministic drift (no `Math.random()`), honest labels
  (`device: "Simulator / Demo"`), loopback-bound `127.0.0.1:<port>`, and a
  `restart(rebootMs?)` that actually takes the port down for a short reboot
  gap, so health polls and `waitOnline` observe the outage like they would on
  a power-cycling controller.
- Registered with the registry as `kind: "simulator"` so the UI always shows
  it for what it is.

## Control Center surface (Phase 3)

The four main areas live in `ui/areas/` and talk to the controller through
`AppCore` + the `window.resonux` IPC contract — never directly:

- **Lights** (`Lights.tsx`) — master brightness slider (live, no reboot) plus
  one card per strip showing its current theme (swatches) and preferred
  effect. Both are `<select>` on purpose: no JSON, no ids, just names.
  Theme/effect changes are applied live through `POST /api/themes/select`
  (with `strip`) and `POST /api/state/effect`; state comes from
  `GET /api/state` + `GET /api/themes`.
- **Music** (`Music.tsx`) — the controller's source list from
  `GET /api/audio/sources` ("Microphone", "Aux input", "Test tone", …) with a
  *Use this* button per source (`POST /api/audio/source`) and an auto-select
  switch that lets the controller follow the best available source.
- **Themes** (`Themes.tsx`) — swatch-first browser of `GET /api/themes`:
  palette bars, base brightness, built-in vs user theme, active highlight,
  *Use everywhere* (global select) and per-strip pin state.
- **Cinematic** (`Cinematic.tsx`) — engine toggle (same as Home), live scene
  status readout (scene, mood, energy/tension/audio bars, boom, link latency),
  companion liveness chip, and a *Run a test burst* button that fires
  `POST /api/cinematic/test` with an empty body (firmware fills in the QA
  defaults). The renderer stays read-only; every action is a named command.

`AppCore` gains thin wrappers for these (`getState`, `getThemes`,
`selectTheme(id, themeId, strip?)`, `setStripEffect`, `selectAudioSource`,
`setAutoSelect`, `triggerCinematicTest`), each mapped to an IPC channel in
`electron/main.ts` and a `window.resonux` method in `electron/preload.ts`.
None of them write config or reboot — they are live-state commands,
mirroring the firmware endpoints' own behaviour.

## Diagnostics & guided hardware check (Phase 4)

`ui/areas/Diagnostics.tsx` replaced the placeholder under Diagnostics. A
**guided hardware check** (`src/diagnostics/check.ts`) walks a non-technical
user through a live, honest, step-by-step verification of their controller —
engine and UI are host-side; it drives only existing firmware REST endpoints,
so nothing firmware-side changed:

1. **Reachability** — `GET /api/status` answers; the controller is online.
2. **Controller health** — `ok`, heap above the floor, uptime sane.
3. **Strips configured** — the rig actually has LEDs to check.
4. **Audio is alive** — `AUDIO_SAMPLES` samples of `GET /api/frame` are
   compared; a varying signal passes, a flat one is a *warn* ("is music
   playing?") rather than a failure — the controller may be fine while the
   music is paused.
5. **Brightness write round-trip** — the app blips the LEDs to a visibly
   brighter level (~900 ms hold), reads the controller back to confirm it took
   the value, then restores. Restore failure is a *warn* (the LEDs are likely
   fine, but something isn't persisting), never a hard fail.
6. **Your turn** — the computer cannot see light. The app hands control to
   the user: *Did the room light up?* Yes/No turns the whole run's verdict.

Each step renders as check/pass/warn/fail with a sentence of plain-language
explanation; the battery runs non-destructively (restores state), targets the
selected controller, and lists an honest overall verdict plus which steps need
attention. The simulator is supported end-to-end and is labelled *Simulator*
in the report, never presented as real hardware. Details and thresholds live
in `src/diagnostics/check.ts`; the busiest real pacing is brief (sub-second
audio spacing with a ~0.9 s blip).

The session event log (in-memory, capped, secret-free by design — see
`src/log/log.ts`) records every significant action (controller discovery,
selection, simulator start/stop, config writes, hardware check outcomes) with
timestamps, seq numbers, and level badges. The structured report export
(`AppCore.exportReport()`) writes the selected controller's hardware check
facts plus the session history to a dated JSON file that never contains network
addresses, Wi-Fi passwords, or credentials — the save dialog lets the user
pick where to store it.

Restart (`POST /api/system/restart`) and power-off (`/power-off`) live behind
a confirmation dialog; restart replies, then the hardware resets, and the app
waits for it to come back on the same port; power-off reports a sleeping
state. Both actions are logged to the session event stream.

## Config writes & first-run (Phase 2)

The wizard (`ui/areas/Wizard.tsx`) is a full-screen, modal guide:
Welcome → Find → Verify → Name → Wi-Fi → Done. Every failure surfaces through
`ErrorDetail` (`ui/components.tsx`): *what happened / why / what to do* /
Try again / an "advanced details" disclosure. It never dumps a stack trace
by default.

- First run is real: `settings.wizardCompleted` is false until the user picks a
  controller or explicitly skips — the snapshot's `wizardNeeded` drives the
  modal. Skipping finishes the wizard (no nagging every boot); Setup re-opens
  it anytime.
- Rename and Wi-Fi hand-off go through `AppCore`, never the renderer:
  - `renameController(id, name)` / `setWifi(id, ssid, password)` do a
    GET-then-PUT round-trip of the **full config document** (the firmware
    stores the PUT body verbatim, so unknown keys survive).
  - Before the write they park a **byte-exact backup** of the current config in
    `userData/backups/config-<id>-<timestamp>.json`, so a later phase can
    replay a failed change (P0 of the web dashboard was an unguarded config PUT).
  - The Wi-Fi password is used in the request only — never logged, never
    written to settings.
  - Both return `ConfigCommandResult { ok, rebootApplied, backupPath, detail }`.
- A real config write reboots the unit, so after an accepted write the wizard
  calls `waitOnline(id, timeoutMs)` (poll the existing registry health — honest
  timeout, no infinite spinner) before claiming success. Wi-Fi uses 60 s (the
  join itself takes a while), rename 20 s.
- The simulator implements `PUT /api/config` honestly (`rebooting: false` —
  it applies instantly, it has no reboot) and offers an AP-mode variant
  (`MockController({ apMode: true })`) that performs the Wi-Fi hand-off it runs
  through the same endpoint a real hotspot-mode unit would. It can also
  `restart()` on the same loopback port, exercising the registry's
  offline→heal path that a real config-reboot triggers.

## Testing (host, no hardware)

`desktop/test` runs via vitest; dev command `npm test` in `desktop/`.

| Suite | Covers |
|---|---|
| `src/domain/discovery.test.ts` (5) | codec round-trip, lenient unknown-key parse, non-responder rejection — keeps the TS port byte-compatible with the C++ codec |
| `src/sim/mock.test.ts` (16) | REST parity: status shape, brightness mutation + 0–255 validation, cinematic toggle, 404s; `PUT /api/config` rename + AP→STA Wi-Fi hand-off; restart on the same port keeping state; system commands: restart drops the port then comes back with the same port + reset state, power-off honestly reports sleeping; **D3**: themes payload/select-global+per-strip/PUT+DELETE/reset, per-strip effect with validation, audio source + auto-select, cinematic test burst |
| `src/client/registry.test.ts` (4) | simulator health → online; going offline clears stale status (identity survives); offline→heal after a restart; removal |
| `src/core/app.test.ts` (15) | full AppCore boot → auto simulator → select → live commands round-trip; simulator stop→drop; rename/setWifi with byte-exact backup; first-run `wizardNeeded` lifecycle; `waitOnline`; **D3**: getState/getThemes/selectTheme (global + per-strip)/setStripEffect/selectAudioSource/setAutoSelect/triggerCinematicTest; **D4**: requestRestart/requestPowerOff (with system state change); exportReport (secret-free JSON, session log); session log populated in snapshot |
| `src/log/log.test.ts` (3) | **D4**: ordering/seq + ISO timestamps; cap at configured size (drops oldest); returned entries are copies (no internal mutation) |
| `src/core/report.test.ts` (4) | **D4**: serialises app/controller/check/log into JSON; excludes addresses, ports, Wi-Fi credentials, and passwords; null check serialises honestly; `suggestedReportName` sanitises the controller name |
| `src/diagnostics/check.test.ts` (6) | **D4**: `blipTarget` bounds; healthy live controller passes every step + restores brightness; unanswerable controller fails fast; flat audio and a write that won't restore are honest *warns*; full `runHardwareCheck` against the in-process simulator passes and labels itself *Simulator* |

**53 host tests** across the seven suites, plus the firmware gate
(`pio test -e native`, 144 tests) and `web` `tsc --noEmit` + `npm run build`.

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

## Packaging & installer

`electron-builder` (NSIS) packages the compiled app into a single-file Windows
installer — the non-technical user's path in:

```powershell
cd desktop
npm run dist        # build + package → release/Resonux Control Center Setup <ver>.exe
npm run dist:dir    # build + unpacked app only → release/win-unpacked (no installer)
```

- The installer is **one-click**, per-user, and drops a desktop shortcut
  (`"nsis": { "oneClick": true }` in `package.json`). No Node, no PlatformIO,
  no toolchain needed on the target machine.
- `electron-builder` only ships what the app needs: `dist/**` (compiled main +
  renderer) and `package.json`, into an asar. No source, no tests.
- The app is **branded** with the *Luminous Node at Resonance* identity (see
  `docs/BRAND.md`): `build/icon.ico` (multi-size, 16–256 PNG entries) is
  embedded in the exe and installer, and the renderer shows the same mark —
  an amber resonant ring with a standing-wave filament igniting into a point
  of light on a dark graphite tile — in the navrail and the first-run wizard.
  Source of truth is `build/logo.svg` + the generator; regenerate the full
  pixel/vector set (icon, ICO, mark, wordmark, lockups, favicon) with
  `powershell -File build\make-icon.ps1` from the `desktop/` directory.
- Auto-update, digital signing, and the tray are deliberately **not** enabled
  yet; the release is unsigned (Windows SmartScreen shows the *"run anyway"*
  prompt), which is fine for a dev/early-access build.

## Phase plan

| Phase | Scope | Status |
|---|---|---|
| **1** | Architecture + client layer + discovery + simulator + app shell (Home/Setup) | **in repo, builds + 14 host tests green** |
| **2** | First-run setup wizard: guided find/verify/rename/Wi-Fi hand-off, byte-exact config backup before writes, reboot-aware reconnect, honest errors | **in repo, builds + 21 host tests green, smoke verified** |
| **3** | Main Control Center: Lights (per strip, by name), Music (sources, plain language), Themes (swatches, global + per-strip), Cinematic basics (engine toggle + scene readout + QA test bursts) | **in repo, builds + 40 host tests green, smoke verified** |
| **4** | Diagnostics + system controls: guided hardware check (live, honest step-by-step), health; session event log (in-memory, secret-free), structured report export (no secrets/addresses), restart/power-off behind confirms | **in repo, builds + 53 host tests green** |
| **5** | Cinematic deep shaping: scenes, moods, comfort, companion supervision as a child process | planned |
| **6** | Firmware & recovery: OTA update, backup/restore, flash recovery via bundled esptool | planned |
| **7** | Polish: installer (electron-builder), auto-update, tray, UX audit vs `.impeccable` review standards | **installer in repo (NSIS, one-click, single-file exe); auto-update/tray/UX audit pending** |

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