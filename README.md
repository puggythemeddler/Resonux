# Universal Music-Reactive LED Controller

One audio-analysis + effects core, interchangeable LED output drivers.
Supported LED families (via replaceable drivers):

| Category | Examples |
|---|---|
| Addressable digital | WS2812B, WS2811, WS2815, SK6812 (RGB + RGBW), APA102, HD107(S) |
| Conventional RGB (non-addressable) | 5 V / 12 V RGB strips (common anode/cathode), PWM/MOSFET |
| Single colour | 5 V / 12 V mono strips, PWM brightness |
| Future | new `LEDDriver` subclass — core untouched |

**Architecture highlights** — `docs/ARCHITECTURE.md`:

- Audio (`AudioSource` → `AudioAnalyzer`) and effects (`Effect` → `LedFrame`)
  never touch hardware; hardware is behind `LEDDriver` implementations.
- Configurable frequency bands (default 20 Hz–16 kHz in 9 bands), dB or
  auto-adaptive normalization, noise gating, attack/release smoothing,
  energy-based beat detection with sensitivity + cooldown, per-band peaks.
- 15 effects (Spectrum Analyzer, Bass Pulse, Beat Flash, Frequency Wave,
  Frequency→Color, Rainbow Music, VU Meter, Energy Pulse, Running Wave,
  High-Frequency Spark, Bass→Treble Gradient, Beat Ripple, Music Wave,
  Color Energy, Custom Mapping).
- Up to 6 independently configurable strips (driver, effect, pins, brightness,
  mapping). S3 dual-core: audio on core 1, LEDs + web on core 0.
- JSON config on LittleFS (`firmware/data` → `/config.json`, versioned).
- Planned: local Wi-Fi dashboard (React + TypeScript, served by the ESP32),
  multi-controller sync, OTA. See `docs/ROADMAP.md`.

## Repo layout

```
/docs        architecture, hardware, BOM, web, testing, roadmap
/firmware     ESP32-S3 firmware (PlatformIO, C++/Arduino)
/web          web dashboard (Vite + React + TypeScript) — Phase 4
/tools/python audio-analysis lab + power calculator (dev tooling)
```

## Phase-1 hardware (prototype)

| Item | Notes |
|---|---|
| ESP32-S3-DevKitC-1 **N16R8** | 16 MB flash + 8 MB PSRAM |
| INMP441 I2S microphone | 24-bit I2S MEMS |
| WS2812B strip, 30–60 LEDs | 5 V, starts short |
| 5 V 3–5 A PSU | separate from the dev board |
| 330 Ω DIN resistor, 1000 µF bulk cap, fuse | see `docs/HARDWARE.md` |

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

## Build (Phase 1 firmware)

```bash
cd firmware
pio run                          # compile
pio run -t upload                # flash via USB-C
pio device monitor -b 115200     # console: band/beat diagnostics every 3 s
```

`platformio.ini` pins Arduino core 2.0.x (proven INMP441 path on S3) and
deps: `FastLED`, `ArduinoJson`, `arduinoFFT`.

## Configure

Defaults match the wiring above. Persistent JSON lives on LittleFS; change it
via `firmware/data/config.json` (upload: `pio run -t uploadfs`), edits are
validated + clamped at load. The web dashboard (Phase 4) will edit this file.

## Testing & tooling

- `tools/python/analyze.py <song.mp3>` — replicate the band/beat pipeline in
  NumPy, produce waveform/spectrum/band/beat/LED-sim visualisations
  (`docs/TESTING.md`).
- `tools/python/power_calculator.py` — PSU/wire/fuse sizing
  (`docs/HARDWARE.md §4`).
- Host unit tests for DSP/effects planned (CI-friendly).

## License / ownership

**All Rights Reserved.** The Resonux project is the exclusive property of
the owner of this repository; no license is granted to copy, modify, use, or
distribute it in any form. Contributions are accepted only as an irrevocable
assignment of rights to the owner. See `LICENSE`.

Status: development scaffold for a **protected product path**: architecture-
first, incremental phases (`docs/ROADMAP.md`). Not yet a commercial product;
no warranty — the electrical limitations in `docs/HARDWARE.md` apply.