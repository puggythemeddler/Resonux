# Resonux

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-red)](https://platformio.org)
[![SoC](https://img.shields.io/badge/SoC-ESP32--S3-blue)]()
[![Arduino Core](https://img.shields.io/badge/Arduino%20Core-2.0.x-informational)]()
[![Languages](https://img.shields.io/github/languages/top/puggythemeddler/Resonux)]()
[![Language count](https://img.shields.io/github/languages/count/puggythemeddler/Resonux)]()
[![License](https://img.shields.io/badge/license-All%20Rights%20Reserved-informational)](LICENSE)

**Music-reactive LED controller** — one audio-analysis + effects core with
interchangeable output drivers. A real-time spectrum/beat engine on
ESP32-S3 driving any common LED strip family **or** DMX moving-head fixtures
over Art-Net.

## Languages & stack

| Language | Where |
|---|---|
| **C++ (C++17)** | ESP32-S3 firmware — PlatformIO + Arduino-ESP32 core 2.0.x |
| **Python 3** | dev tooling: audio-analysis lab, PSU/wire sizing |
| **HTML / CSS / TypeScript** | web dashboard (Vite + React, implemented) |

Key libraries: **FastLED** (LED output), **arduinoFFT** (DSP), **ArduinoJson**
(config).

## What it does

- **Audio input** — INMP441 I2S MEMS mic (24-bit, 44.1 kHz).
- **Analyzer** — configurable frequency bands (default 20 Hz–16 kHz in 9),
  dB or auto-adaptive normalization, noise gating, attack/release smoothing,
  energy-based beat detection (sensitivity + cooldown), per-band peaks.
- **15 effects** — Spectrum Analyzer, Bass Pulse, Beat Flash, Frequency Wave,
  Frequency→Color, Rainbow Music, VU Meter, Energy Pulse, Running Wave,
  High-Frequency Spark, Bass→Treble Gradient, Beat Ripple, Music Wave,
  Color Energy, Custom Mapping.
- **Themes** — data-driven lighting personalities persisted to `/themes.json`
  on LittleFS: global palette, brightness/saturation, per-band response and
  animation character. **18 built-in themes** — Classic, Party, Bass Heavy,
  Spectrum, Rainbow, Club, Chill, Rock, EDM, Vocal, Beat, Ambient, Fire,
  Ocean, Cyberpunk, Classical, **Afro House** (warm/organic/groove-driven,
  low-mid-heavy) and **Auto** (hysteresis-based classifier that switches on
  sustained feature votes). Apply globally or per strip, custom effects per
  strip, and edit from the dashboard; built-ins reinstall on reset.
- **Touchscreen UI** — optional LVGL GUI on a TFT panel (reference: 3.5″
  SPI ILI9488 + FT6236 capacitive touch). Now / Themes / System screens,
  screen timeout that never affects the LEDs, hardware-independent
  display/touch abstraction, build via the `esp32-s3-ui` PlatformIO env.
- **LED drivers (interchangeable)**:

  | Category | Examples |
  |---|---|
  | Addressable digital | WS2812B, WS2811, WS2815*, SK6812 (RGB; RGBW white pending), APA102, HD107(S)† |
  | Conventional RGB | 5 V / 12 V RGB strips (common anode/cathode), PWM/MOSFET |
  | Single colour | 5 V / 12 V mono strips, PWM brightness |
  | Future | a new `LEDDriver` subclass — core untouched |

  * `WS2815` uses the WS2812 controller (same 800 kHz protocol); † `HD107(S)`
  uses the APA102 controller (compatible SPI part). FastLED needs compile-time
  pins, so addressable data pins are limited to `1,2,3,5-21,33-42,47,48`
  (default GPIO48); APA102 clock pins to `4,8,10,13,15,18,33,38,47,48`.

- Up to **6 independent strips** (driver, effect, pins, brightness, mapping).
  S3 **dual-core**: audio on core 1, LEDs + web on core 0.
- **Json config** on LittleFS (`firmware/data` → `/config.json`, versioned).
- **Art-Net DMX output** — optional; drives club-style moving heads & LED
  pars over WiFi (no extra hardware). Audio-reactive: pan/tilt swing on
  amplitude + bass, color from bass/beat, strobe on beat. See
  `docs/ARTNET.md`.
- **Multi-controller sync** — optional UDP-multicast audio sync: one *master*
  runs the mic and broadcasts its analysis; *slaves* (no mic needed) render
  the identical frame with clock-offset + frame-counter lock, so whole rooms
  of strips stay beat-locked over plain WiFi. See `docs/MULTI_CONTROLLER.md`.

See `docs/ARCHITECTURE.md` for the full design.

## Repo layout

```
/docs         architecture, hardware/electrical + BOM, web, testing, roadmap
/firmware      ESP32-S3 firmware (PlatformIO, C++/Arduino) — Phase 1
/web          web dashboard (Vite + React + TypeScript) — Phase 4
/tools/python  audio-analysis lab + power calculator (dev tooling)
```

## Prerequisites

**Hardware (Phase 1):**

| Item | Notes |
|---|---|
| ESP32-S3-DevKitC-1 **N16R8** | 16 MB flash + 8 MB PSRAM |
| INMP441 I2S microphone | 24-bit I2S MEMS |
| WS2812B strip, 30–60 LEDs | 5 V, starts short |
| 5 V 3–5 A PSU | separate from the dev board |
| 330 Ω DIN resistor, 1000 µF bulk cap, fuse | see `docs/HARDWARE.md` |

**Software:**

- [PlatformIO CLI](https://platformio.org/install/cli) (or VS Code + PlatformIO
  extension)
- Python 3.8+ — only for the dev tools (`tools/python/`)
- Optional: `ffmpeg` + `numpy`/`scipy`/`matplotlib` for the analysis lab

### Wiring (Phase 1)

```
ESP32-S3              INMP441             WS2812B strip
GPIO4  ──────────►    SCK (BCLK)
GPIO5  ──────────►    WS  (LRCLK/WS)
GPIO6  ──────────►    SD  (DATA)
GND    ──────────►    GND (mic)  +  strip GND  +  PSU GND
3V3    ──────────►    VDD (mic)   L/R ──► GND (left chan)
GPIO48 ──330Ω──►      DIN
5V PSU (+5) ──►       strip +5V  (with 1000µF + 0.1µF across +5/GND)
```

**Safety:** the 5 V PSU powers the strip only. GPIOs carry signals, never LED
power. Read the electrical guidance in `docs/HARDWARE.md` before scaling up.

## Quick start

```bash
cd firmware
pio run                          # compile default sizing (no touchscreen)
pio run -e esp32-s3-ui           # build with LVGL touchscreen UI
pio test -e native               # host-side unit tests (no hardware, 37 tests)
pio run -t upload                # flash via USB-C — requires the board
pio device monitor -b 115200     # console: band/beat diagnostics every 3 s
```

Build + embed the web dashboard (only needed the first time, or after editing
`web/`):

```bash
cd web
npm install
npm run build                    # outputs firmware/data/web (flashed to SPIFFS)
cd ../firmware
pio run -t uploadfs              # flash the dashboard + config.json
```

Run the dashboard **without hardware** (mock `*/api` server):

```bash
cd web
npm run dev                      # http://localhost:5173 — simulated device
```

First run creates **AP-mode** defaults (`Resonux` hotspot, no password). Join
it from a phone/PC and browse to `http://192.168.4.1/` for the dashboard, or
set `net.staSsid`/`net.staPassword` in `firmware/data/config.json` to join
your own network (STA mode) and browse to `http://<ESP-ip>/`.
`platformio.ini` pins Arduino core 2.0.x (proven INMP441 path on S3) and
FastLED **3.9.0** (compile-time pins — see the addressable note above). The
build uses a **dual-partition OTA table** and the flash filesystem is SPIFFS.

### Web dashboard

A modern React dashboard served from the device — no cloud. Open `http://<ESP-ip>/`
from any device on the same network:

- **Live** — real-time spectrum bars, amplitude/bass/mid/treble levels,
  beat indicator, system stats (uptime, heap, FPS), live global tuning
  (master brightness + sensitivity — applied instantly, no reboot).
- **Themes** — browse and edit the device's lighting themes (palette swatches,
  brightness base/min, response incl. lowMid/highMid, animation flash/contrast/
  density, preferred effects); select globally or per strip with an effect
  override, create/edit/delete non-built-ins, reset defaults.
- **Configuration** — view/edit the full `config.json` and save (reboots).
- **System** — status readouts, panel backlight + timeout, **Restart** and
  **Safe Power Off** (clean shutdown that silences outputs, saves config and
  deep-sleeps the device; see `docs/SYSTEM.md`).
- **Firmware Update** — upload a `firmware.bin` over the air (HTTP) or flash
  from the Arduino IDE (ArduinoOTA is active too).

Dashboard and touchscreen share the same controller state — the REST API
(`/api/state*`) reflects exactly what the touchscreen reads and writes, so
either surface can drive the other. See `docs/WEB_DASHBOARD.md`.

### Touchscreen (optional)

Build with the `esp32-s3-ui` PlatformIO env to link LVGL and enable the
display/touch drivers. Three screens — **Now** (theme + spectrum + beat),
**Themes** (one-tap theme selection, same state as web) and **System**
(master + screen brightness, timeout cycle, status line, **Restart** and
**Safe Power Off** behind confirmation overlays). Screen brightness and
timeout are configured live from either surface (`display` block in
`config.json`) and are fully independent of LED output. See
`docs/TOUCHSCREEN.md`.

### Art-Net (moving heads)

Art-Net is **off by default**; enable it in the `artnet` section of
`config.json`, then list fixtures. The ESP32-S3 joins your WiFi and broadcasts
a 512-channel DMX universe.

```json
"artnet": {
  "enabled": true,
  "ssid": "your-network",
  "password": "your-password",
  "universe": 0,
  "audioReactive": true,
  "panSpeed": 0.5,
  "tiltSpeed": 0.5,
  "colorSensitivity": 1.0
},
"fixtures": [
  { "profileId": 1, "dmxAddress": 1, "count": 2 }
]
```

Fixture profiles: `0` off, `1` 8-ch moving head, `2` 16-ch moving head,
`3` 4-ch LED par. Every configured fixture is driven from the same audio
analysis; no DMX transceiver is required (Art-Net is sent over WiFi). See
`docs/ARTNET.md`.

## Configure

Persistent JSON on SPIFFS (LittleFS-like) at `/config.json`, validated +
clamped at load. Edit it in the browser dashboard (**Configuration** tab →
**Save & reboot**), or directly in `firmware/data/config.json` + `uploadfs`.
Serial console prints a short `[diag]` line every 3 s (fps, amplitude,
bass/mid/treble, beat, free heap).

## Testing & tooling

- `pio test -e native` — **host-side unit tests** for the pure logic (colour
  math, palettes, smoothing, effect rendering, LED frame ops); runs in CI.
- `python tools/python/analyze.py song.mp3` — replicate the band/beat pipeline
  in NumPy; produces waveform/spectrum/band/beat/LED-sim visualizations
  (`docs/TESTING.md`).
- `python tools/python/power_calculator.py` — PSU/wire/fuse sizing
  (`docs/HARDWARE.md §4`).
- `cd web && npm run dev` — dashboard **simulator** (mock `/api` endpoints,
  no hardware required).

## Documentation

- `docs/ARCHITECTURE.md` — system design & data flow
- `docs/HARDWARE.md` — electrical, wiring, power (read before scaling up!)
- `docs/ARTNET.md` — Art-Net DMX output (moving heads, fixtures)
- `docs/BOM.md` — bill of materials (Phase 2)
- `docs/TESTING.md` — test strategy & tools
- `docs/WEB_DASHBOARD.md` — Phase 4 dashboard spec
- `docs/TOUCHSCREEN.md` — LVGL touchscreen UI (Phase 10)
- `docs/MULTI_CONTROLLER.md` — multi-controller sync (Phase 9)
- `docs/PRODUCTISATION.md` — carrier PCB + enclosure + CE (Phase 10)
- `docs/SYSTEM.md` — system controls: restart, safe power-off, display control
- `docs/BENCH.md` — hardware bring-up checklist (turnkey steps when parts land)
- `docs/ROADMAP.md` — phase plan & status

## Status

Development scaffold for a **protected product path**: architecture-first,
incremental phases (`docs/ROADMAP.md`). Phase 1 (analyzer + effects + LED
drivers + runtime) plus Art-Net DMX output, the **web dashboard** (Live /
Themes / Configuration / System / OTA tabs), **18 themes** (data-driven,
device-side + dashboard editing incl. Afro House + Auto classifier), **OTA**,
**WifiManager**, **multi-controller multicast sync** (master/slave +
clock-offset lock), **system controls** (graceful restart, safe power-off
via deep sleep, live sensitivity/backlight/timeout endpoints), **host-side
unit tests in CI** (43 pass), and the **LVGL touchscreen UI**
(hardware-independent display/touch abstraction) are implemented and compile
cleanly for ESP32-S3 (~40 % RAM / ~58 % flash at the default config, ~64 %
flash with the touchscreen env); hardware bring-up is pending parts arrival
(see `docs/HARDWARE.md`).

## License / ownership

**All Rights Reserved.** The Resonux project is the exclusive property of
the owner of this repository; no license is granted to copy, modify, use, or
distribute it in any form. Contributions are accepted only as an irrevocable
assignment of rights to the owner. See `LICENSE`.

No warranty — the electrical limitations in `docs/HARDWARE.md` apply.