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
| **HTML / CSS / TypeScript** | web dashboard (Phase 4, planned) |

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
pio run                          # compile (no hardware needed)
pio run -t upload                # flash via USB-C — requires the board
pio device monitor -b 115200     # console: band/beat diagnostics every 3 s
```

First run creates WiFi-less defaults matching the wiring above. Change them
in `firmware/data/config.json` and upload with `pio run -t uploadfs`.
`platformio.ini` pins Arduino core 2.0.x (proven INMP441 path on S3) and
FastLED **3.9.0** (compile-time pins — see the addressable note above).

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

Persistent JSON on LittleFS, validated + clamped at load. The web dashboard
(Phase 4) will edit it from the browser; values also take effect from
`pio device monitor` — Serial console prints a short `[diag]` line every 3 s
(fps, amplitude, bass/mid/treble, beat, free heap).

## Testing & tooling

- `python tools/python/analyze.py song.mp3` — replicate the band/beat pipeline
  in NumPy; produces waveform/spectrum/band/beat/LED-sim visualizations
  (`docs/TESTING.md`).
- `python tools/python/power_calculator.py` — PSU/wire/fuse sizing
  (`docs/HARDWARE.md §4`).
- Host-side DSP/effect unit tests planned (CI-friendly).

## Documentation

- `docs/ARCHITECTURE.md` — system design & data flow
- `docs/HARDWARE.md` — electrical, wiring, power (read before scaling up!)
- `docs/ARTNET.md` — Art-Net DMX output (moving heads, fixtures)
- `docs/BOM.md` — bill of materials (Phase 2)
- `docs/TESTING.md` — test strategy & tools
- `docs/WEB_DASHBOARD.md` — Phase 4 dashboard spec
- `docs/ROADMAP.md` — phase plan & status

## Status

Development scaffold for a **protected product path**: architecture-first,
incremental phases (`docs/ROADMAP.md`). Phase 1 (analyzer + effects + LED
drivers + runtime) plus Art-Net DMX output is implemented and **compiles
cleanly for ESP32-S3** (~28 % RAM / ~46 % flash at the default config);
hardware bring-up is pending parts arrival (see `docs/HARDWARE.md`), with the
web dashboard planned for Phase 4.

## License / ownership

**All Rights Reserved.** The Resonux project is the exclusive property of
the owner of this repository; no license is granted to copy, modify, use, or
distribute it in any form. Contributions are accepted only as an irrevocable
assignment of rights to the owner. See `LICENSE`.

No warranty — the electrical limitations in `docs/HARDWARE.md` apply.