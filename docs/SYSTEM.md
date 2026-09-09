# System Controls — Restart, Safe Power Off, Display Control

Status: **implemented, builds green** (both firmware envs, host tests). Bench
pending (hardware not yet arrived).

## Why

- `POST /api/reboot` used to call `ESP.restart()` directly — LEDs could be
  left on mid-frame and config was never flushed.
- There was no way to power a controller down safely (deep sleep) with clean
  outputs.
- Screen brightness/timeout and per-strip sensitivity were only settable via a
  full `PUT /api/config` — which reboots. That made "live tuning" reboot the
  controller on every import, wiping the audio state with it.

`SystemMode` + the new live endpoints fix all three with one consistent state
machine.

## State machine (`firmware/src/system/SystemMode.h`)

Header-only, free of OS/hardware deps so it is host-testable
(`firmware/test/test_system`).

```
Running ── requestRestart() ──► RestartRequested
Running ── requestPowerOff() ─► PowerOffRequested
RestartRequested / PowerOffRequested ── beginShutdown() ─► ShuttingDown
ShuttingDown ── complete() ─► Restarting | Sleeping (terminal; device restarts/deep-sleeps)
```

- Idempotent: a second request while a shutdown is already requested/active is
  ignored (`tryRequestShutdown()` returns false). The touchscreen and the web
  dashboard can both fire without stacking shutdown tasks.
- `mode()` / `stateName()` are the source of truth for `/api/system/status`,
  `/api/status.system` and the LVGL System screen status line.
- `ledLoop()`, WebUi and `ArtNetNode` all gate on `shuttingDown()` so no code
  path re-arms outputs during the grace window.

## Graceful shutdown sequence (App)

`App::requestRestart()/requestPowerOff()` spawn a shutdown task:

1. `SystemMode::beginShutdown()` — LED runtime stops stepping; outputs stay
   in their cleared state.
2. `App::extinguishLeds()` — every strip driver is cleared and shown
   (FastLED `show()` once with black).
3. Art-Net: `ArtNetNode::blackout()` pushes one zeroed DMX frame
   (`blackoutSet`.update) so fixtures fade to black, then `stop()` stops the
   UDP/dimmer duty.
4. Sync (`SyncNode::stop()`) halts master/slave traffic.
5. `ConfigStore::save()` — best-effort flush (bounded wait, never blocks
   shutdown). The display settings (brightness/timeout/wakePin) were already
   saved live by `DisplayManager`/`ConfigStore::setDisplay` on every change.
6. ~500 ms grace period lets the web client's in-flight POSTs ack.
7. **Restart** → `ESP.restart()`.
   **Power off** → screen backlight off, optional `wakePin >= 0` arms
   `esp_sleep_enable_ext0_wakeup()` (RTC GPIO 0–21 only, see `docs/HARDWARE.md`),
   then `esp_deep_sleep_start()`.

> Honest semantics: Safe Power Off enters **deep sleep** — it does not cut
> the physical supply. Without a `wakePin` configured the device needs a
> press of the reset button (EN) to power back on. The dashboard and
> touchscreen confirmations say exactly this.

## API

| Method & path | Body | Effect |
|---|---|---|
| `GET /api/system/status` | — | `{ok, state, action}` |
| `POST /api/system/restart` | `{}` | graceful restart (graceful path; `/api/reboot` kept as alias) |
| `POST /api/system/power-off` | `{}` | graceful deep sleep (same shutdown sequence, terminal Sleeping) |
| `POST /api/state/sensitivity` | `{value}` (0–5) | live per-strip sensitivity, saved + applied without reboot |
| `POST /api/state/backlight` | `{value}` (0–100) | live display backlight, saved + applied without reboot |
| `POST /api/state/timeout` | `{value}` (0–86400 s, 0=never) | live screen timeout, saved + applied without reboot |

`/api/status` now also reports:

- `system: {state, action}`
- `display: {enabled, backlightPct, timeoutS, awake, wakePin}`

## Display controls (independent of LED output)

- **Screen brightness** — `display.backlightPct` (0–100). Separate from LED
  master brightness (`/api/state/brightness`) and per-theme brightness.
- **Screen timeout** — `display.screenTimeoutS` (0 = never). Only dims/stops
  the panel; audio capture, FFT, LED rendering, Wi-Fi and Art-Net keep
  running. Cleared on touch, re-armed on timeout; backlight returns to the
  configured level on wake.
- Persisted live via `ConfigStore::setDisplay`/serialization (no reboot
  needed to survive power cycle).

## Web dashboard

- **Live → Global Tuning**: Master brightness + Sensitivity use the live
  endpoints only (`/api/state/*`). The 3-second full-config-PUT loop was
  removed — live tuning no longer reboots the controller.
- **System tab**: status readouts (state/action, uptime, heap, strips, Wi-Fi,
  Art-Net, sync), Display card (backlight slider + timeout select), Power
  card (Restart / Safe Power Off, each behind a confirmation modal).
- Restart lifecycle: while restarting the poll loop shows a banner
  (`Restarting…` → `Device offline — waiting…`), and clears when a fresh
  `/api/status` reports `running`. Power-off shows a full-screen modal
  explaining the wake requirement.
- Deleting a theme / resetting themes / saving config all require a
  confirmation; config save also pre-validates JSON client-side.

## Touchscreen (LVGL System tab)

- Master + screen brightness sliders, timeout cycle button
  (Never / 30 s / 60 s / 2 min / 5 min / 10 min), status line (wifi mode,
  system state, heap, uptime, Art-Net), Restart button, Safe Power Off
  button — restart/power-off confirmations are full-screen overlays with an
  explicit "outputs will be silenced" message.
- Layout is a scrollable flex column, so it adapts to smaller panels.

## Output-safety checklist

- [x] LED strips cleared + shown before action.
- [x] Analog/single-colour drivers zeroed via clear.
- [x] LED stepper gated off during shutdown (no stale frame re-arms LEDs).
- [x] Art-Net blackout frame sent, then fixture traffic stopped.
- [x] Sync master/slave stopped.
- [x] Config best-effort flushed.
- [x] Grace period for client ACKs before the deep operation.