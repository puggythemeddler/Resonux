# Hardware Bench Checklist — Phase 1 bring-up and beyond

Status: **scripted, awaiting parts.** This is the turnkey order of operations
when the hardware lands. Each row lists concrete pass criteria; "not-ready"
means stop and read the linked doc before continuing.

Build/flash/watch commands (from `firmware/`):

```
pio run -e esp32-s3 -t upload        # flash default (web dashboard build)
pio run -e esp32-s3-ui -t upload     # flash touchscreen build
pio device monitor                   # serial: [boot]/[diag] lines = pass criteria
```

## 0. Bench station

| Item | Notes |
|---|---|
| 5 V 10 A bench PSU (or USB-C + phone charger for Phase 1) | never power the strip from the dev board rail |
| ESP32-S3-DevKitC-1 + USB-C data cable | flash + serial console |
| INMP441 I2S breakout | 3.3 V, mic to phone speaker |
| WS2812B 60 LED/m, 1–2 m | S3 GPIO48 (see `docs/HARDWARE.md`) |
| Logic-level N-MOSFET stage (IRLZ34N + gate resistor/pulldown) | for Phase 6/7 |
| Second S3 + router/AP | Phase 9 two-board sync |
| Multimeter | current clamp optional |

Breadboard first; only move to the Phase 10 carrier PCB once every row below
passes on loose parts.

## 1. Flash + boot (Phase 1 gate)

1. `pio run -e esp32-s3 -t upload`, then `pio device monitor`.
2. Watch the serial log; every `[boot]`/`[wifi]`/`[web]`/`[strip]` line from
   `src/runtime/App.cpp` must reach `[boot] system ready - strips=N`.

**Pass:** no `init FAILED`; `[strip] [0] ... WS2813 driver=... effect=... segs=...`,
AP comes up in default config, dashboard reachable at the printed URL with the
admin token from `config.json`.

## 2. Wi-Fi + web dashboard (Phase 4 gate)

1. Open `/api/status` — device, heap, fps, wifi mode/ip, artnet, sync fields all
   present.
2. Live tab: spectrum/levels/beat update at ~60 fps; `[diag]` lines
   (`fps=... amp=... beat=...`) stream in serial.
3. Themes tab: 18 themes listed; /api/themes CRUD round-trips.
4. Config tab: edit → PUT → serial shows `config saved via PUT ... rebooting`,
   boot line `[cfg] config loaded`.
5. OTA tab: upload a firmware.bin → `[ota] update OK, rebooting`.

**Pass:** every tab works over STA IP with token; config survives reboot.

## 3. Microphone + FFT (Phase 1 core)

1. Phone (or headphone-out) plays a bass-heavy playlist at arm's length (~30 cm).
2. Watch `[diag]`: `amp`, `bass`, `mid`, `treble` and `beat=1` should pulse with
   the music, not sit flat.

**Pass:** `beat=1` fires on kick/beat drops; `bass` tracks; no mic hiss floor at
idle. **Troubleshoot:** `docs/HARDWARE.md` I2S wiring table, or the
AudioSource diag mode in `firmware/src/local/`.

## 4. Strip render (Phase 1 gate)

1. Set `stripCount=1`, choose Spectrum then Bass Pulse in `/api/themes`.
2. Physically verify the pattern travels along the LEDs with the music.

**Pass:** clean, stable visual response on 30–60 LEDs with no data corruption,
reset artifacts or flicker; brightness slider affects all LEDs.
**Troubleshoot:** `docs/HARDWARE.md` strip section + swapped data/ground pins.

## 5. Effects + themes cross-check (Phase 2/8c)

Loop all 12 effects and the 18 themes via the dashboard/user-config and confirm
each renders distinctly; audio-reactive ones (Spark, Energy Pulse, Beat Flash,
... ) respond to beat/bass as described in `docs/TESTING.md`. Auto classifier
must switch themes on genre changes with the documented hysteresis/dwell.

## 6. Conventional + single-colour strips (Phase 6/7)

1. Wire one RGB zone + one monotone channel through the MOSFET stage.
2. Enable the matching driver, map zones→bands, run the spec §37 bass/mid/
   treble sweep.

**Pass:** red/mid, green/mid, blue/treble respond to the sweep; mono channel
pulses with bass; gates stay off at boot (pulldowns verified).

## 7. OTA + rollback (Phase 8)

1. Flash v1 via USB → upload v2 over OTA → confirm v2 boots.
2. Use the rollback partition (or the fallback image) and confirm it returns to
   v1 after an invalid flash / watchdog.

**Pass:** both `[ota]` paths complete; brick-and-restore works.

## 8. Multiple strips (Phase 5)

Two independent strips on separate GPIOs with different effects/themes
selected per strip.

**Pass:** each strip renders its own effect simultaneously; per-strip
`/api/state*` reflects the right frame.

## 9. Art-Net DMX fixture sweep (Phase 8b)

1. `artnet.enabled=true` → serial `[artnet] node started (universe=..., fixtures=...)`.
2. Point a DMX universe at the fixture (or a second S3 running the par/moving
   head profile) and send audio→DMX mapping.

**Pass:** moving head pans/tilts with the beat; `ArtPoll` acknowledged (fixture
status shows `active` in `/api/status`).

## 10. Touchscreen UI (Phase 8d, `esp32-s3-ui` env)

ILE9488 + FT6236 wired per `docs/TOUCHSCREEN.md`; flash the `-ui` env.

**Pass:** `[display] panel up: ... touch=...`; Now/Themes/System tabs navigate;
theme code + brightness edits mirror `/api/state*` (same source of truth as
the web dashboard).

## 11. Two-board sync (Phase 9)

1. Both S3s on the same LAN; A = master (mic), B = slave (`sync.role=slave`).
2. Master: `[sync] synthetic up: 239.x.x.x:...` then `[sync] node active (master)`.
   Slave: `[sync] node active (slave)`; serial log shows `audio: mic skipped
   (sync slave ...)` — mic off by design.
3. Watch both strips side by side with a visual beat.

**Pass:** both rigs flash at the same beat; slave `/api/status` shows
`sync.role=slave, masterAlive=true, offsetMs` converging to a small jitter;
slave stays lit if the master is alive and mutes/times out per
`masterAlive` when it vanishes. Kill the master → slave drops frames within
the configured timeout, then recovers when it returns.

## 12. System controls (Phase 11)

1. `/api/system/status` returns `{ok:true, state:"running", action:"none"}`.
2. **Live tuning, no reboot:** change master brightness, screen backlight and
   timeout via the web System tab / Global Tuning sliders (or
   `POST /api/state/brightness|backlight|timeout`). Serial stays boot-clean
   (no reboot), values survive a power cycle (persisted `display`/tuning
   config).
3. **Restart:** `POST /api/system/restart` (or the web Restart button behind
   its confirmation modal) — serial logs the shutdown sequence, all LED/DMX
   outputs go silent *before* reset, the device boots fresh and the dashboard
   reconnects (`Device offline — waiting.` → `Running`).
4. **Safe Power Off:** `POST /api/system/power-off` (or the panel button) —
   backlight off, `wakePin ≥ 0` arms `esp_sleep_enable_ext0_wakeup()` on an
   **RTC GPIO (0–21)**, then deep sleep. Without a `wakePin` the reset (EN)
   button wakes it. Measure standby current to confirm the strip rail is off.
5. **LVGL System screen mirrors the web:** backlight slider + timeout select
   map to `/api/state*`; Restart / Safe Power Off overlays confirm and silence
   outputs first.

**Pass:** outputs never glitch during restart/power-off; live tuning never
reboots; wake works per `wakePin` config; both UIs share the same
`SystemMode` source of truth.

## 13. Universal device detection + audio source selection

The controller now exposes a capability-based device registry (`DeviceManager`,
`src/device/`), a RESO_DISCOVER multicast responder/scanner, persisted trusted
rows (`/devices.json`), and an audio-source selector.

1. **Boot rows:** serial shows `[device] manager: N rows, responder=1`. The web
   Devices & Sources tab lists `resonux:self`, `local:mic`, `local:led`
   (configured builtins) with capability chips.
2. **Active source:** with default config the mic is the input; `SOURCE_TEST`
   selects the built-in test tone and `SOURCE_NONE` idles. Change source via
   the web selector (`POST /api/audio/source`) and confirm the analyser feeds
   the LEDs after the next boot. With `autoSelectSource` on, hysteresis
   (1.5 s hold / 2.5 s dwell) picks preferred-vs-fallback at boot.
3. **Scan:** `POST /api/devices/scan` (web Scan button) probes the multicast
   group; a second S3 flashed with this firmware answers
   `RESO-DISCOVER-RESP` and appears as *detected*. Detection never drives
   outputs.
4. **Identify / configure / remove:** bind a scanned row with a builtin
   profile — the AM-006 validation profile must show "compatible /
   conditioning required ⚠" and *never* `safe_to_connect` — then Remove deletes
   the persisted row. All mutations persist across reboots.
5. **Theme cross-fades:** set `themeTransitionMs` (default 500); switching
   themes (or an AUTO stage change) must cross-fade over that window instead of
   snapping. `0` restores the legacy instant switch.

**Pass:** the second S3 is discovered without touching `config.json`; a test
tone drives the bar graph with no mic attached; un-known devices stay
`Detected`/`needs_investigation`; theme switches fade smoothly.

## Safety / power notes

- Common ground between PSU, strips and S3; never run strip current through the
  dev board.
- Fuse each output (BOM inline 3 A/5 A); verify polarity protection before
  connecting reverse-prone strips.
- At full white 2 m of 60/m WS2812 ≈ 7 A — use the bench PSU, not USB.