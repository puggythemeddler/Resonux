# Touchscreen UI — LVGL (Phase 10)

Status: **code in repo, builds green** for `esp32-s3-ui`. Hardware bring-up
pending panel arrival. The design is hardware-independent so it targets a
range of TFT sizes and waits behind `config.json`.

## How it's built

```
AudioFrame ─► ThemeEngine ─► ThemeFrame ─► EffectEngine ─► strips (always running)

Display Driver (DisplayDriver/DisplayManager) ◄── LVGL (TouchUi) ──► App::setTheme / setMasterBrightness
Touch Driver (TouchDriver/NoTouchDriver)
```

The touchscreen is a **peer** of the web dashboard, not a second brain. It
reads the same `AudioFrame` and writes through the same `App` controller
state (`setTheme`, `setMasterBrightness`) that the REST API drives — so
`/api/state`, the web dashboard, and the panel always agree.

## Build & run

```bash
cd firmware
pio run -e esp32-s3-ui          # links LVGL 8.3.11 + enables ENABLE_TOUCHUI
pio run -e esp32-s3             # default build: same UI compiles to no-ops, no LVGL
```

The default env does not link LVGL; every display/touch source is
flag-gated so base flash stays small.

## Hardware (reference 3.5" SPI module)

| Function | Pin | Default |
|---|---|---|
| ili9488 SCK  | SPI  | `12` |
| ili9488 MOSI | SPI  | `11` |
| ili9488 CS   |      | `10` |
| ili9488 DC   |      | `9`  |
| ili9488 RST  |      | `14` |
| Backlight    | LEDC | `21` (screen only — separate from LED master) |
| FT6236 SDA   | I2C  | `8`  |
| FT6236 SCL   | I2C  | `3`  |
| FT6236 IRQ   |      | `34` |

All pins live under `display` in `config.json` — choose pins for your board
and upload; no firmware edit needed.

```json
"display": {
  "enabled": true,
  "panel": 1,
  "touch": 1,
  "orientation": 0,
  "spiSck": 12, "spiMosi": 11, "spiMiso": 13,
  "csPin": 10, "dcPin": 9, "rstPin": 14, "blPin": 21,
  "touchSda": 8, "touchScl": 3, "touchIrq": 34,
  "backlightPct": 70,
  "screenTimeoutS": 60,
  "uiFps": 30
}
```

`panel`: 0 = none, 1 = ili9488 (320×480), 2 = ili9341, 3 = st7789.
`touch`: 0 = none, 1 = ft6236, 2 = xpt2046. Orientation 0 = portrait.

## Screens

- **Now** — active theme label, 8 spectrum bars, beat flash indicator.
- **Themes** — one row per theme; tap to select (text-only, no images — stays
  small). Uses the same `ThemeEngine` global selection as the web.
- **System** — master LED brightness slider (0–255), screen-timeout note.

Rendering is driven by `DisplayManager` at a controlled rate (`uiFps`);
touch is polled from `ledLoop` at 30 ms and inverse-mapped to logical
coordinates via `DisplayManager::mapPoint`, so a 320×480 reference UI
scales to other panels.

## Behavioural guarantees

- Screen **timeout / backlight are screen-only** — audio, LEDs, web, OTA all
  keep running when the display sleeps.
- No large allocations, JSON, or FS work inside the real-time LED loop.
- Changing theme or brightness on the panel changes persisted controller
  state — the web dashboard reflects it on next poll, and vice versa.