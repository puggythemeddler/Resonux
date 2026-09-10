# Music-Reactive LED Controller — System Architecture

This document is the master architecture for a **universal** music-reactive LED
controller: one audio-analysis + effects core, and *interchangeable* LED output
drivers covering addressable strips, conventional RGB strips, single-colour
strips, and future hardware.

The firmware is written so that **the audio engine and the effects engine never
talk to physical hardware**. They talk to an abstract per-strip framebuffer
(`LedFrame`) of normalized RGB segments. Concrete drivers
(`AddressableLEDDriver`, `AnalogRGBDriver`, `SingleColorDriver`, future drivers)
translate a normalized frame into protocol/PWM signals.

---

## 1. Layered architecture

```
                    +----------------------------------------------+
                    |              WEB CONTROL PANEL (Ph 4)         |
                    |   status / spectrum / config / presets / save |
                    +----------------------------------------------+
                                     | config JSON
                                     v
                    +----------------------------------------------+
                    |             CONFIG SYSTEM (Ph 3)             |
                    |   LittleFS JSON  /  defaults  /  validation  |
                    +----------------------------------------------+
                                     |
                                    config
                    +---------------+---------------+
                    |                               |
                    v                               v
      +---------------------------+   +----------------------------+
      |      AUDIO INPUT LAYER    |   |      LED OUTPUT LAYER      |
      |   AudioSource (interface) |   |  LEDDriver (interface)     |
      |   - I2S INMP441           |   |  - AddressableLEDDriver    |
      |   - (future) line-in, BT  |   |  - AnalogRGBDriver         |
      +---------------------------+   |  - SingleColorDriver       |
                    | samples           |  - (future) DMX, TLC5940  |
                    v                   +----------------------------+
      +---------------------------+             ^
      |     AUDIO ANALYSIS        |             |
      |   windowing / FFT / dB    |             |
      |   per-band normalization  |   +---------------------------+
      |   smoothing, noise gate   |   |     LIGHTING EFFECTS      |
      |   beat + peak detection   |   |  Effect (interface) x12   |
      +---------------------------+   |  render(LedFrame,Audio)   |
                    |                   +---------------------------+
                    v                     ^        |
      +---------------------------+       |        |
      |      AudioFrame (pub)     |   +--------+   |
      |  shared state, double-    |   |LedFrame|   |
      |  buffered, mutex-guarded  |   +--------+   |
      +---------------------------+       |        |
                                          |   normalized RGB
                          +----------------v----------+
                          |  StripRuntime (per strip)  |
                          |  owns driver+effect+frame  |
                          +---------------+------------+
                                          |
                                          v
                          +---------------------------+
                          |   HARDWARE-SPECIFIC IO    |
                          |   FastLED/RMT . LEDC PWM  |
                          +---------------------------+
                                          |
                                          v
                          +---------------------------+
                          |        LED HARDWARE       |
                          | WS2812x, SK6812, APA102,  |
                          | RGB MOSFET strips, 1-col  |
                          +---------------------------+
```

### Golden rules enforced by this layout

1. Effects receive `AudioFrame + LedFrame`. They never include an LED driver
   header, never call `FastLED`, never touch a GPIO.
2. Drivers receive a *normalised per-segment RGB vector*. They never know what
   an "effect" or a "beat" is.
3. Audio analysis has no idea LEDs exist. `AudioAnalyzer` publishes an
   `AudioFrame`; consumers subscribe.
4. Adding a new LED chipset/technology = adding one new `LEDDriver` subclass.
   Adding a new audio input = adding one new `AudioSource` subclass.
   Adding a new effect = adding one `Effect` subclass + registry entry.
   No existing code is modified.

---

## 2. Recommended hardware — ESP32-S3

**Chosen part: `ESP32-S3-WROOM-1-N16R8`** (16 MB flash + 8 MB octal PSRAM),
used on a **DevKitC-1-N16R8** class board.

Justification:

| Requirement             | ESP32 (classic) | ESP32-S2 | ESP32-C3 | **ESP32-S3** (chosen) |
|-------------------------|:---:|:---:|:---:|:---:|
| Cores                   | 2   | 1   | 1   | **2** (audio + LED/Web independent) |
| Max clock               | 240 | 240 | 160 | **240** |
| SRAM                    | 520K| 320K| 400K| **512K** |
| GPIOs                   | 34  | 43  | 22  | **45** |
| PSRAM on-module         | –   | –   | –   | **up to 8 MB** (web + multi-strip buffers) |
| I2S                     | yes | yes | yes | **2x I2S** (mic out, LED parallel out) |
| Wi-Fi                  | yes | yes | yes | **yes** |
| BLE                     | yes | yes | yes | **BLE 5** |
| LEDC PWM (analog strips)| yes | yes | yes | **yes (6 HS + 8 LS)** |
| SIMD for DSP            | no  | no  | no  | **yes (PIE, faster FFT)** |
| USB-OTG (config/CLI)    | no  | yes | no  | **yes** |
| FastLED parallel strips | via I2S | – | – | **16 via LCD-CAM / 4 via RMT** |

- **Why not ESP32-C3:** single core — audio, Wi-Fi, and LED PWM would fight for
  one core; no module PSRAM; fewer PWM channels.
- **Why not ESP32-S2:** single core; I2S parallel-LED output blocked; FastLED
  docs explicitly exclude it for high strip counts.
- **Why ESP32-S3 over classic ESP32:** two full user cores *and* native
  parallel LED capability (LCD-CAM I80), plus USB-C native flashing and more
  GPIO/PWM headroom for later analog strips.

---

## 3. Audio input recommendation

**Phase 1 / prototype:** `INMP441` I2S MEMS microphone.

- I2S digital output (no ADC noise), −26 dBFS sensitivity, 61 dB SNR,
  omnidirectional, 3.3 V, cheap (~$2), extremely common.
- Wire: `SCK→GPIO4`, `WS→GPIO5`, `SD→GPIO6`, `L/R→GND` (left channel),
  `VDD→3V3`, `GND→GND`.

Future sources share the same `AudioSource` interface: line-level ADC input,
Bluetooth A2DP Sink (music you're already streaming), I2S line-in codec
(ES8388/ES8311), or a host-PC UDP audio stream.

---

## 4. LED categories supported by the driver interface

| Category | Example parts | Driver | Control |
|---|---|---|---|
| Addressable digital | WS2812B, WS2811, WS2815*, SK6812 (RGB; RGBW white pending), APA102, HD107(S)† | `AddressableLEDDriver` | FastLED (RMT on ESP32-S3) |
| Conventional RGB (non-addressable) | 5 V / 12 V RGB strips, common anode or cathode | `AnalogRGBDriver` | 3x LEDC PWM per physical zone |
| Single colour | 5 V / 12 V mono strips | `SingleColorDriver` | 1x LEDC PWM per physical zone |
| Future | DMX-512, dot-matrix, TLC5940, dumb-addressable | new `LEDDriver` subclass | whatever it needs |

* `WS2815` runs on the `WS2812` controller (same 800 kHz clockless protocol);
† `HD107(S)` runs on the `APA102` controller (compatible SPI part). The
compiled GPIO pin sets are listed in §4.1.

### Segment (logical light) model

Every driver exposes a **segment count** — the number of independently
controllable *logical lights* the strip presents to the effects:

- addressable strip → `segmentCount == ledCount` (one segment per physical LED);
- conventional RGB strip with N physical cut-sections → `segmentCount == N`
  (each section = one zone, one RGB triple of MOSFETs);
- single-colour strip with N sections → `segmentCount == N`.

Effects render a `LedFrame` of that many segments; they do not need to know
which case they are in. A "spectrum" on an addressable strip uses dozens of
segments; on a 3-zone RGB strip a `customMapping` effect maps the 3 zones to
your chosen bands. Same effect engine, same code path.

---

## 5. LED driver architecture (universal abstraction)

```cpp
class LEDDriver {
public:
  virtual ~LEDDriver() {}
  virtual bool   begin() = 0;                 // init hardware & buffers
  virtual void   clear() = 0;
  virtual void   setPixel(int index, uint8_t r, uint8_t g, uint8_t b) = 0;
  virtual void   show() = 0;                  // commit frame to hardware
  virtual int    segmentCount() const = 0;
  virtual bool   isAddressable() const = 0;   // per-LED independent control
  virtual uint8_t channelCount() const = 0;   // 1, 3 or 4
  virtual void   setMasterBrightness(uint8_t b) = 0;
  virtual void   setPowerLimit(float maxA, float volts) = 0; // best-effort sw limit
  virtual const char* driverName() const = 0;
};
```

- `clear()` sets all segments off.
- `show()` is the only place a driver touches the hardware bus/PWM.
- Drivers are created by a **factory** from stored `StripConfig` (never by the
  effect engine, never by the audio task).

### Addressable driver (FastLED backend)

FastLED provides the low-level protocol encoding (RMT on ESP32-S3) for the
one-wire family (WS281x, SK6812) and SPI family (APA102). FastLED requires
**compile-time** chipset × colour-order × GPIO values, so `AddressableLEDDriver`
instantiates every supported (chipset, order, pin) triple and selects it at
runtime from config:

| Chipset   | Controller used | Orders instantiated |
|---|---|---|
| WS2812    | `WS2812` | GRB, RGB |
| WS2811    | `WS2811` | GRB, RGB |
| WS2815    | `WS2812` (same 800 kHz protocol) | GRB, RGB |
| SK6812    | `SK6812` | GRB, RGB |
| SK6812 RGBW | `SK6812` — FastLED 3.9.0 exposes no native white channel; `ORDER_RGBW` maps to GRB (RGBW = future work) | GRB |
| APA102    | `APA102` (SPI enum) | GRB, RGB, BRG, BGR |
| HD107S    | `APA102` (compatible SPI part) | GRB, RGB, BRG, BGR |

Addressable **data pins** are limited to the compiled set `1, 2, 3, 5-21,
33-42, 47, 48`; SPI **clock pins** to `4, 8, 10, 13, 15, 18, 33, 38, 47, 48`.
A configured pin outside these sets makes `begin()` fail cleanly (driver left
inactive). Extend the pin lists in `AddressableLEDDriver.cpp` to add a GPIO.

Supporting per-strip: `ledCount`, data pin (+clock pin for SPI parts), colour
order, reverse, brightness cap, per-strip power/current software limit
(`FastLED.setMaxPowerInVoltsAndMilliamps` with a per-strip budget), and a
frame-rate cap.

### Analog RGB driver (LEDC PWM)

- Each physical zone owns three PWM channels (R/G/B) via ESP32-S3 LEDC
  (16-bit). Extra zones use further channel triples (S3: 6 HS + 8 LS channels).
- `commonAnode` inverts the duty (active-low). Zone count and zone→band mapping
  live in `StripConfig`.
- **Never drives strip power from a GPIO.** GPIO → gate of a logic-level N-MOSFET
  → strip segment's ground; strip power comes from the PSU rail (see
  `HARDWARE.md`).

### Single-colour driver (LEDC PWM)

One PWM channel per zone; an effect's RGB is reduced to perceived luminance.

---

## 6. Audio processing architecture

```
I2S DMA (continuous, 2 DMA bufs x N frames)
  → AudioSource.readSamples(float*, max)   [portable float stream]
  → ring staging in AudioAnalyzer
  → hop by hopSize samples: copy fftSize window
  → windowing (Hamming)
  → FFT (ArduinoFFT, float)
  → complex → magnitude
  → per-band bin aggregation
  → dB normalisation  (dbFloor..dbCeil → 0..1)
  → noise gate (< threshold → 0)
  → attack/release smoothing per band
  → beat detection (energy vs running avg+variance)
  → per-band peak tracking (fast attack, slow decay)
  → publish AudioFrame
```

Defaults: 44.1 kHz, FFT 1024 (`≈43 Hz` bins), hop 512 → ~41 analysis
frames/sec (~24 fps of unique data). On the LX7 cores an 1024-pt float FFT is
~1–3 ms; the hop interval is ~11.6 ms, leaving abundant headroom.

### Frequency bands — fully configurable

The nine default bands are exactly as specified, but only a *default*:

| band | range | group |
|---|---|---|
| 0 | 20–60 Hz | bass |
| 1 | 60–120 Hz | bass |
| 2 | 120–250 Hz | lowMid |
| 3 | 250–500 Hz | mid |
| 4 | 500 Hz–1 kHz | mid |
| 5 | 1–2 kHz | highMid |
| 6 | 2–4 kHz | highMid |
| 7 | 4–8 kHz | treble |
| 8 | 8–16 kHz | treble |

`bandCount`, each `BandDef{lo,hi}` and the five `groupRanges[5][2]`
(bass/lowMid/mid/highMid/treble index ranges) are stored in config JSON and
re-built at boot into **FFT bin index ranges**. Nothing is hard-coded.

```cpp
struct AudioFrame {
  float  bands[kMaxBands];   // 0.0–1.0, per configurable band (smoothed)
  float  peaks[kMaxBands];   // 0.0–1.0, per-band peak-hold
  uint8_t bandCount;
  float  amplitude;          // 0.0–1.0 overall (RMS of full frame)
  float  bass, lowMid, mid, highMid, treble;  // named groups
  bool   beat;               // beat event this frame (cooldown-gated)
  float  beatStrength;       // 0.0–1.0
  uint32_t timeMs;
  // (Phase 2+: beatKind bitmask — bass kick vs. percussion)
};
```

An effect may consume any or all of these; effects' `EffectParams` also carry
`zoneToBand[]` so an effect can implement the user's "zone 1 = bass…" layout.

---

## 7. Beat & peak detection

- **Beat:** energy method — instantaneous energy of the low bands vs. a slowly
  tracked running average and variance; trigger threshold
  `C = 1.5171 − 0.002571·var` (classic dynamic-threshold beat detector), gated
  by a min-energy floor, a configurable sensitivity multiplier, and a ~240 ms
  re-trigger cooldown. `beatStrength` is how far above threshold the hit was.
- **Peaks:** per-band fast-attack/slow-decay peak tracking (feeds spark,
  VU-meter and spectrum effects).
- **Smoothing:** exponential attack/release per band (`attack/release` config),
  the classic attack/release en-velope around audio-reactive LEDs.

---

## 8. Effects engine

```cpp
class Effect {
public:
  virtual ~Effect() {}
  virtual const char* name() const = 0;
  virtual void begin(LedFrame& frame, const EffectParams& p) {}
  virtual void render(LedFrame& frame, const AudioFrame& a, const EffectParams& p) = 0;
};
```

Effects are stateless wrt hardware; any persistent visual state lives in the
effect object (allocated per strip). Effects render pure `Rgb` into `LedFrame`
and may read any `AudioFrame` field or `EffectParams` (bands, group levels,
amplitude, beats, peaks, palette, brightness range, sensitivity, hue
speed, zone→band mapping).

### The twelve initial effects

| # | Effect | Drives |
|---|--------|--------|
| 1 | Spectrum Analyzer | per-pixel bar chart, band→pixel mapping, peak dots |
| 2 | Bass Pulse | whole-strip brightness spike on low-band energy |
| 3 | Beat Flash | full flash on beat events, fast decay |
| 4 | Frequency Wave | travelling wave whose crest/colour is band-driven |
| 5 | Frequency-to-Color | dominant band chooses hue, amplitude sets brightness |
| 6 | Rainbow Music | rotating rainbow, brightness = amplitude + beat spikes |
| 7 | VU Meter | mirrored level bar, green→yellow→red ramp |
| 8 | Energy Pulse | two pulses radiate from centre, amplitude-driven |
| 9 | Running Wave | continuous sine hue wave modulated by energy |
| 10 | Spark / High-Frequency | treble-triggered particles with gravity/decay |
| 11 | Bass-to-Treble Gradient | hue band per-pixel, level-scaled brightness |
| 12 | Custom Mapping | user-defined segment→band + palette (the zone layout effect) |

All twelve are implemented in Phase 2 of the roadmap but the engine + effects
are part of this codebase from day one (they are hardware-independent and cheap
to build while hardware work continues).

### Palettes & colour helpers

Effects use small portable HSL→RGB and palette helpers (`util/Color.h`) rather
than calling into FastLED's colour system, keeping the effects layer
platform-independent. Palettes: Heat, Rainbow, Ocean, Red-Blue, Party (5).

---

## 9. Multiple-strip architecture

```cpp
struct StripConfig {
  char   name[12];
  int    driverType;             // addressable | analogRGB | singleColor
  int    chipset;                // WS2812 …
  int    colorOrder;             // GRB …
  int    dataPin, clockPin;
  int    ledCount;
  bool   reverse;
  int    zoneCount;              // analog/single strips
  StripZone zones[kMaxStripZones]; // {pins[3], nPins, pos01, band}
  bool   commonAnode;
  float  pwmFreqHz;
  int    effectId;
  uint8_t maxBrightness, minBrightness;
  float  maxCurrentA;
  float  maxVolts;
  uint8_t palette;
  uint8_t startHue;
  float  hueSpeed, sensitivity, decay;
  uint8_t targetFps;
};
```

- `kMaxStrips = 6` strips, each fully independent (its own driver, effect,
  params and framebuffer), connected and rendered by `StripRuntime::step()`.
- Every strip has its own effect, brightness, palette, mapping and pins.
- Many LED tasks share the one audio analysis output — the value of the
  decoupling.
- Phase 5 expands addressable count: RMT (4 lanes) in Phase 1; the
  **LCD-CAM I80 driver** (up to 16 parallel strips, needs PSRAM + specific
  build flags) is documented as the Phase 5/9 route.

---

## 10. Power & current architecture

- **Software power limit (best effort):**
  - FastLED `setMaxPowerInVoltsAndMilliamps(volts, mA_budget)` for addressable
    strips, plus per-frame per-strip scaling with `nscale8`.
  - Analog/single PWM drivers expose `setMaxCurrentA` (duty ceiling derived from
    V/I), used in later phases.
- **Electrical reality (please read — this is not optional):** software limiting
  is *cosmetic*; physical protection is required (see `HARDWARE.md` §4):
  correctly-sized PSU, fuses, wire gauge, star grounding, power injection,
  decoupling caps, MOSFET logic-level switching, common ground, level shifting
  for 5 V-logic APA102 data where needed, reverse polarity protection where
  appropriate. At no point does a GPIO drive LED power.

Hardware current math (300-LED WS2812 example): ~60 mA/LED at full white
≈ 18 A → triple/quad power injection, 2.5 mm² wiring, 20 A fuse, 5 V/25 A PSU.

---

## 11. Software project structure

(Full repo layout incl. `web/` + `tools/`: see **§26**.)

```
G:\LED project\
├── README.md
├── docs\
│   ├── ARCHITECTURE.md   <-- this document
│   ├── HARDWARE.md       <-- electrical design & wiring & MOSFET guidance
│   ├── BOM.md            <-- bill of materials
│   └── ROADMAP.md        <-- phase plan / status
└── firmware\             (PlatformIO project, target esp32-s3-devkitc-1)
    ├── platformio.ini
    └── src\
        ├── main.cpp                    entry, App::begin + idle loop
        ├── util\
        │   ├── Rgb.h                   portable RGB + HSL + palette helpers
        │   ├── Smoother.h              attack/release smoothing
        │   └── Log.h                   tiny serial logger
        ├── audio\
        │   ├── AudioSource.h           interface (future inputs plug in here)
        │   ├── I2SMicSource.h/.cpp     INMP441 over legacy I2S driver
        │   ├── AudioAnalyzer.h/.cpp    FFT, bands, dB norm, smoothing
        │   ├── BeatDetector.h/.cpp     energy beat + peak trackers
        │   └── AudioFrame.h            published analysis result
        ├── led\
        │   ├── LEDTypes.h              enums, zone structs, constants
        │   ├── LEDDriver.h             universal output interface
        │   ├── addressable\AddressableLEDDriver.h/.cpp   (FastLED,RMT)
        │   ├── analog\AnalogRGBDriver.h/.cpp             (LEDC PWM 3ch/zone)
        │   ├── single\SingleColorDriver.h/.cpp           (LEDC PWM 1ch/zone)
        │   └── factory\LEDDriverFactory.h/.cpp
        ├── effects\
        │   ├── Effect.h                interface + EffectId enum
        │   ├── EffectParams.h          shared effect knobs
        │   ├── LedFrame.h              segment framebuffer
        │   ├── EffectRegistry.cpp/h    id ⇄ Effect creation
        │   └── fx\                     one file per effect
        ├── config\
        │   ├── ConfigDefs.h            kMaxBands/kMaxStrips/… global limits
        │   ├── Config.h                Config/StripConfig/AudioAnalyzerConfig
        │   ├── ConfigStore.h/.cpp      LittleFS JSON load/save
        │   └── ConfigDefaults.h        the documented default config
        └── runtime\
            ├── StripRuntime.h/.cpp     driver+effect+frame per strip
            └── App.h/.cpp              ownership, tasks, mutexed AudioFrame
```

---

## 12. Concurrency & FreeRTOS model (Phase 1)

| Task | Core | rate | role |
|---|---|---|---|
| `audioTask` | 1 | 41 Hz (hop) | read I2S → window → FFT → bands → beat → publish |
| `ledTask` | 0 | per audio frame (cap `targetFps`) | render strips... |
| loop() | 1 | idle (1s delay) | watchdog/back-compat only |

- Publishing: **double-buffered `AudioFrame`** swapped under a short FreeRTOS
  mutex. Audio task fills buffer A while LED task reads buffer B; swap gives
  the LED task a coherent frame. No blocking of the audio pipeline beyond the
  swap critical section.
- Future phases add a `webTask` on core 0 (Wi-Fi) sharing core 0 with the LED
  task — strictly non-blocking LED work keeps timing safe. Waiter: `vTaskDelay`
  based pacing first, DMA-completion pacing in Phase 9.

### Memory budget (Phase 1, S3 with 8 MB PSRAM)

| Item | Size |
|---|---|
| I2S DMA buffers (8×512, int32) | 32 KB |
| FFT real + imag (1024 floats) | 8 KB |
| window + staging | 12 KB |
| 60-LED addressable buffer (CRGB) | 180 B |
| 12 effect instances (state) | < 8 KB |
| effect param tables | < 2 KB |
| config structs | < 4 KB |
| Arduino core + Wi-Fi (future) heap floor | PSRAM |
| **Total static** | **~60 KB (of 512 KB SRAM)** — 8 MB PSRAM free for web |

### Timing budget (LX7 @ 240 MHz)

| step | cost |
|---|---|
| 1024-pt float FFT | ~1–3 ms |
| band aggregation | < 0.1 ms |
| effects (60 LEDs) | < 1 ms |
| FastLED RMT push (60 px) | ~0.3 ms (DMA) |
| **per frame** | **≤ 5 ms**, hop window 11.6 ms → <50% core load, etc. |

---

## 13. Configuration

Single JSON document (`/config.json` on LittleFS) mirroring `Config`. Sections:

- audio (sample rate, FFT size, gain, dbFloor/dbCeil, noise gate, attack,
  release, beat sensitivity, band table, group ranges)
- strips[] (everything in §9)
- master brightness, device name, (Ph 4: wifi ssid/pass, presets)

Load at boot with validation; invalid/missing → sensible defaults and a rewrite
to disk. The web dashboard (Ph 4) edits exactly this document and triggers a
safe "apply" (effects/params can hot-swap; deep changes like FFT size request a
reboot).

---

## 14. Libraries (deliberately few)

| Library | Use | Why |
|---|---|---|
| `fastled/FastLED` @ 3.9.0 | addressable LED protocol | the addressable back-end; pinned 3.9.0 — compile-time pins, see §4.1 |
| `bblanchon/ArduinoJson` @ ^7 | config (de)serialization | standard, deterministic, no dynamic deps |
| `arduinoFFT` @ ^2 | FFT | small, portable float FFT, puredsp-free |

Planned (Phase 4+): `ESPAsyncWebServer` (HTTP+SSE, non-blocking), `async-mqtt`
are *not* needed — the web layer uses the Arduino core's async web server and
plain SSE; if ESPAsyncWebServer proves brittle on S3, fall back to the built-in
`WebServer` (single-task, documented trade-off). Web assets are static files.

Everything else (bands, dB, beat, peaks, smoothing, effects, zone mapping,
PWM drivers, web serving later) is our own code — exactly the parts that must
be configurable/portable. FastLED and the FFT are the only chip-specific
dependencies, and each is sealed behind our own interface. WiFi/HTTP uses the
Arduino-ESP32 built-ins (Ph 4).

### Abstraction boundaries in code (compile-enforced)

- `effects/` may include `audio/AudioFrame.h` + `util/*` only.
- `led/` may include `config/Config.h` (types) + FastLED/PWM + `led/LEDDriver.h`.
- `audio/` may include `config/ConfigDefs.h` + FFT + its own types only.
- `runtime/App` is the only place that ties the three together.

---

## 15. Phase map (see ROADMAP.md for status)

| Phase | Content | Status |
|---|---|---|
| 1 | ESP32-S3 + INMP441 + one addressable strip, FFT/bands/basic response | **in codebase** |
| 2 | effects engine + 12 effects | in codebase |
| 3 | JSON config persistence (LittleFS) | in codebase (basic) |
| 4 | Wi-Fi + local web dashboard + presets | planned |
| 5 | multiple addressable strips (RMT then LCD-CAM parallel) | planned |
| 6 | conventional RGB strips (PWM/MOSFET) — drivers present | drivers present, wiring doc |
| 7 | single-colour strips — driver present | driver present, wiring doc |
| 8 | extra protocols (DMX, more chipsets) | planned |
| 9 | performance/memory optimisation (esp-dsp SIMD FFT, DMA pacing) | planned |
| 10 | PCB/productisation (devices, connectors, ISO security) | planned |

---

## 16. Key architectural decisions, summarised

1. **S3 (dual-core) over smaller chips** — decouples audio cadence from LED/Web
   cadence without priority-inversion pain.
2. **Own portable `Rgb`/palette layer, not FastLED's** — effects stay
   hardware-agnostic and MCU-portable; FastLED is sealed behind one driver.
3. **Segment model unifies the LED universe** — addressable pixels, RGB zones,
   and mono zones are all just "segments"; effects don't branch on hardware.
4. **Double-buffered AudioFrame over queues** — cheap, coherent snapshots,
   no back-pressure on the audio pipeline.
5. **Config-first DSP** — FFT params, band table, mappings, smoothing and beat
   constants all live in JSON; code never hard-codes them.
6. **Legacy I2S driver (core 2.0.x) pinned for INMP441** — the new IDF-5 I2S
   driver had reproducible S3/PSRAM DMA-init failures (see research); the
   interface is isolated so it can be swapped without touching analysis code.
7. **FastLED chosen as the protocol engine** — battle-tested timing for the full
   WS/SK/APA families, RMT on S3, and a documented parallel LCD-CAM upgrade
   path; our driver layer is the seam through which it is replaced.
8. **Effects computed at 41 Hz, throttled to `targetFps`** — smooth motion with
   a budget ten times smaller than the frame window.
9. **Universal capability detection, never hard-coded IDs** — every device is
   a `DeviceProfile` row (kind/caps/status/classifier); detection *classifies*
   but never drives outputs. Unknown devices stay `Detected` /
   `needs_investigation`. AM-006 exists as a validation profile whose
   `needsConditioning` flag forces "compatible / conditioning required".
10. **Local-first device registry + discovery** — the `DeviceManager` persists
    trusted rows (`/devices.json`) and runs a RESO_DISCOVER multicast
    responder/scanner (`239.255.42.10:9770`); scans register peers without
    clobbering existing trust.
11. **Boot-time audio source selection with hysteresis** — `SourceSelector`
    runs once at boot (pipeline is built once): preferred source wins if
    available, else fallback, else the configured default. Live switching is
    deferred to next boot rather than faked.
12. **Theme cross-fades, still pure** — `ThemeBlend.h` (host-tested, no Arduino)
    is driven by `ThemeEngine`; `themeTransitionMs` (default 500) fades instead
    of snapping, `0` keeps the legacy instant switch. Uses the AUTO stage name
    as cache key so auto stages take their label's blend.
13. **Honest confidence, not "detected = known"** — every device row carries a
    tri-layer `Confidence` (identity / capability / integration) computed from
    trust state: user-confirmed rows score 100/100 (integration 60 if
    conditioning is required), profile-matched 100/100, manual-only 85/85, and
    mere network observations 40/40/0. The web shows the real numbers instead
    of implying compatibility.
14. **Capabilities stay bit-addressed catalogs** — bit position *is* the catalog
    index (`capabilityAt(i)`), so `/api/devices` advertises `capCatalog` +
    `connCatalog` and the web builds capability masks as `1 << i`. There is no
    second capability enum duplicated in the UI and no wire-endian drift.
15. **Audio-source availability derives from trusted devices** — a source is
    selectable when a persisted registry row enables it
    (`deviceEnablesSource(caps, kind)`: mic→microphone, jack→line-in, USB,
    Bluetooth, network, and any `audio_input` device for "Device"). The boot
    responder answers `active` source + `reason` (`sync_slave`, `test_tone`,
    `auto_preferred/auto_fallback/auto_none`, `configured`) so the UI explains
    *why* a source is in use rather than silently switching.
16. **Manual identification is first-class** — `DeviceManager::declare()`
    accepts a user-asserted id/name/connection/capability mask. A clean
    declaration becomes `safe_to_connect`; one flagged "conditioning required"
    stays `compatible` until levels are verified. It carries an explicit
    `SRC_MANUAL` source tag so it is never confused with a profile match.
17. **One wire codec, host-tested** — `DiscoverProtocol.h` is a pure encoder/
    decoder (no Arduino) used by the responder and future host tools; the
    parser is lenient (unknown keys ignored) so older responders and newer
    scanners interoperate (see §16b).

> **Electrical safety is a hard requirement, not software.** Power limiting
> caps *visible* brightness/current in software; fuses, correct PSU sizing,
> MOSFETs and wiring protect the hardware. GPIOs carry signals, never strip
> power. Explicit guidance: `HARDWARE.md`.

---

## 16b. RESO_DISCOVER wire protocol (v1)

A single UDP datagram, multicast on `239.255.42.10:9770`.

- **Probe** (sent by scanners, `UDPMulti` socket): `"RESO_DISCOVER v1\n"`
- **Reply** (sent by every Resonux controller's `DeviceManager`):

```
RESO-DISCOVER-RESP id=<id> name=<name> kind=<conn> caps=<n> source=<s> fw=<v> role=<r>
```

Rules, implemented once in `device/DiscoverProtocol.h`:

- Plain `space` between tokens, `key=value`, values with spaces encode spaces as
  `_` (e.g. `name=Node_Lights`) so the token stream round-trips.
- `kind` is a `ConnectionType` ident (`network`, `bluetooth`, `usb`, …);
  unknown idents fall back to `network` on decode for forward compatibility.
- `caps` is the decimal capability bitmask.
- `source` is the decimal `DiscoverySource`; invalid values default to
  `SRC_NETWORK`.
- The parser ignores unknown keys (older responders vs newer scanners) and
  requires `id=` for a datagram to be considered valid.
- All string fields are bounded by `kMaxDeviceIdLen` / `kMaxDeviceNameLen` /
  `kMaxFwLen` / `kMaxRoleLen`; a truncated reply simply carries shorter values.
- The scanner merges the result via `DeviceManager::processInbound`, which
  never clobbers an existing persisted/trusted row's status, profile, or
  conditioning flag — rescanning a configured device cannot downgrade it.

---

## 17. Direct audio input (Option B)

The platform is not mic-dependent. All inputs implement `AudioSource`; the
enabled one is chosen by the `audio.input.mode` config field.

| Mode | Practical | Notes |
|---|---|---|
| `i2s_mic` (Phase 1) | INMP441-class I2S MEMS | ambient sound from speakers/TV/DJ rig |
| `a2dp_sink` (Ph 5+ extension) | ESP32 as Bluetooth A2DP sink | phone/PC streams the *same* music being played — cleanest consumer "direct" path, no room reverb |
| `i2s_line` (Ph 9) | I2S line codec (ES8311/ES8388) with 3.5 mm in | true analog line-in from mixer/PC — professional route |
| `udp_stream` (dev tool) | PC → ESP32 raw PCM over Wi-Fi/UDP | lab testing without wiring |

Building a VOIP-fashion capture around A2DP-sink gives direct digital access to
the streaming PCM (no DAC/ADC round trip), which is the highest-quality
microphone-free path for phones.

---

## 18. Multi-controller & network synchronisation (Phase 9)

```
 main controller (audio)                       slave controllers
 ┌─────────────────────┐   UDP multicast    ┌─────────────────────┐
 │ mic or line-in      │   (224.0.x.y:port) │                     │
 │ AudioAnalyzer       │ ─────────────────► │ (skip own mic;       │
 │ effect per strip    │   AudioFrame.packed│  render same frame)  │
 └─────────────────────┘   @ ~20–40 Hz      └─────────────────────┘
```

- Roles: `standalone | master | slave` (config `network.role`).
- The master serialises the already-normalised `AudioFrame` (≈150 bytes) plus a
  monotonic frame counter into a UDP multicast packet per frame. Bandwidth
  ≈ 6 kB/s — negligible on Wi-Fi.
- Slaves run the exact same `EffectEngine` configs, so identical frames →
  rhythm-synchronised output. A shared wall-clock (simple NTP-ish sync or
  `millis`+round-trip offset) keeps phase drift bounded; the frame counter
  allows frame-aligned swaps.
- Each controller still drives its own LEDs and still works fully standalone
  if the network disappears.
- Alternatives documented but deferred: DMX-centred timing over WiFi sync, and
  wired sync over RS-485 for theatrical-grade timing.

---

## 18b. Art-Net DMX output (moving heads over Wi-Fi)

Full details: `docs/ARTNET.md`. Summary:

- **Why Art-Net:** DMX512 fixtures (moving heads, LED pars) use DMX-512/A.
  Over Wi-Fi we send *Art-Net* UDP (port 6454) — no transceiver, no cabling —
  the same packets a lighting console sends.
- **Module:** `src/artnet/ArtNetNode` — its own FreeRTOS task (core 0):
  - joins the configured SSID (STA, DHCP or static), reconnects on drop,
  - owns a 512-channel DMX output buffer, broadcasts ArtDmx ~40 Hz,
  - answers ArtPoll so consoles/discovery find the node,
  - can also *receive* ArtDmx (controller drives its universe).
- **Fixture profiles:** `src/artnet/FixtureProfile.h` — named channel maps
  (8-ch / 16-ch moving head, 4-ch LED par). Each config entry assigns
  `profileId + dmxAddress + count`; consecutive addresses are auto-filled.
- **Audio-reactive mapping:** the same `AudioFrame` that feeds LED effects
  drives the DMX universe — amplitude→dimmer, bass+beat→pan/tilt swing,
  hue from bass+beat→RGB, beat→strobe/prism.
- **Integration:** `App::audioLoop` publishes frames to the node; config
  (`artnet` + `fixtures` sections) persists in LittleFS and is off by default.
- Boundaries preserved: the node never touches LEDs/effects; it only consumes
  `AudioFrame` and writes DMX bytes.

---

## 19. Web dashboard architecture (TypeScript + React)

Full details: `docs/WEB_DASHBOARD.md`. Summary:

- **Served by the ESP32, no cloud.** The compiled single-page app (small bundle,
  embedded into LittleFS) is served by the ESP32 web server; the browser talks
  only to the local controller.
- **Stack:** Vite + React + TypeScript. Minimal deps (a few hooks helpers, no
  heavy UI framework) so the bundle stays small enough to embed
  (~200–400 KB gzip target). **Implemented** — see `docs/WEB_DASHBOARD.md`;
  build outputs to `firmware/data/web`.
- **Firmware side:** Arduino-ESP32 `WebServer` + `ArduinoJson` exposure of the
  config; live audio over short-interval `GET /api/frame` polling (SSE planned
  later) carrying `AudioFrame` snapshots; REST for status/frame/config/reboot
  and OTA. Web traffic runs on its own task/core and never blocks the audio or
  LED tasks.
- **Sections (built):** Live · Configuration · Firmware Update. Future pages:
  LED Setup, Audio, Effects, Frequency Mapping, Strips, Presets, Power,
  System, Diagnostics.
- **Preview:** future HTML-canvas strip renderer using the streamed frame.

---

## 20. OTA updates (implemented)

- **Two methods, both active:**
  - **HTTP** — `POST /api/ota` in the dashboard uploads a `firmware.bin`
    (streamed into the inactive app partition via `Update.h`, then reboots).
  - **ArduinoOTA** — standard ArduinoOTA on hostname `resonux`, so you can
    flash from Arduino IDE / `pio run -t upload` over Wi-Fi.
- **Partition scheme:** `firmware/partitions_ota.csv` — `app0`/`app1` OTA
  partitions (3.5 MB each), `otadata`, NVS, and a 960 KB SPIFFS holding the
  dashboard + config. Boot proceeds normally; marking the new image valid and
  rollback-on-failure is handled by the ESP-IDF OTA machinery.
- **Hardening (future):** admin token + size/hash validation + config
  compatibility gate before unlocking — add after bench bring-up.

---

## 21. Security model

- AP mode defaults to WPA2 with a printed random SSID/passphrase; STA mode uses
  the home network.
- All state-changing API calls require an admin token (printed once at first
  boot / set by the user); the dashboard does not expose secrets.
- Input validation both in the web client and on the firmware (range-checked
  enums, pin whitelist, current limits) — invalid config is rejected, never
  half-applied (apply → validate → commit → reboot-if-needed).
- No cloud service is used for normal operation; nothing "phones home".
- OTA end-to-end guarded as above.

---

## 22. Configuration versioning & migration

- `/config.json` carries a `version` field. Current schema `v1`.
- `ConfigStore::load()` runs upgrade steps `v→v+1` if a newer version is read,
  then persists. Old files always migrate forward; new fields fall back to
  defaults; unknown fields are preserved for forward-compat round-trips.
- Presets are a versioned array in the same file:
  `presets[{name, icon, configPatch}]` with save/load/duplicate/delete.

---

## 23. Power calculation utilities

- `tools/python/power_calculator.py` (dev tool, output printed + saved):
  - PSU sizing: `count × mA_per_LED → Amps → × voltage → Watts` (+20–30 % margin).
  - Wire gauge by current and run length, voltage drop per segment, recommended
    injection points for strips of `N` LEDs.
  - Fuse sizing guidance for the segments.
- Docs: full worked examples in `docs/HARDWARE.md §4` (incl. 300-LED and 12 V
  RGB cases).
- These utilities are advisory; hardware behaviour (fuses/PSUs) is the real
  limit — see the electrical disclaimer in §10.

---

## 24. Testing strategy

Full plan: `docs/TESTING.md`. Four levels:

1. **Host unit tests (x86/CI)** — the DSP core (`bands`, `dB`, `normalize`,
   smoothing, beat) and `effects/` are written Arduino-free so they build and
   test on the host with a tiny harness; enable via a build flag
   (`UNIMLED_TESTING=1`) and keep `AudioSource`/`LEDDriver` as pure virtual
   seams — letting tests inject synthetic audio and a "null" LED driver.
2. **Python audio lab (`tools/python/`)** — load MP3/WAV → replicate the band &
   beat pipeline in NumPy → compare against firmware maths on the same stimulus;
   produce waveform/spectrum/band/beat/LED-sim visualisations. Used to evolve
   algorithms before porting (spec §40).
3. **Hardware test mode** — on-demand LED test (red/green/blue/white/full/
   individual LED / zone / strip), mic level & FFT diagnostic, and the web
   Diagnostics page; designed for wiring troubleshooting (spec §22, §31).
4. **Firmware + API integration tests** — config CRUD round-trip tests, preset
   ops, pin-validation rejects, and simulated-audio → effect → null-driver
   smoke tests (partial; full in-host runs from CI).

---

## 25. Performance targets (real-time contract)

| Metric | Target |
|---|---|
| Audio analysis rate | ~41 frames/s (1024-pt FFT @ 44.1 kHz, hop 512) |
| FFT compute | ≤ 3 ms on LX7 @ 240 MHz |
| Full audio+fx+LED budget | ≤ 8 ms/frame avg, worst-case < 12 ms |
| LED update | 30 FPS min, 60 FPS addressable target (Phase 9 tune) |
| Web live stream | 10–20 Hz snapshot, zero effect on audio/LED path |
| Non-blocking rule | no `delay()`/blocking I/O in audio or LED tasks; web on own task |

Rule: **priority chain** `audio → LED → web`. Web serves best-effort; it may
drop frames, it may not stall patterns. Wi-Fi stacks up to ~200 ms of TX may
occur on core 0; LED code there stays non-blocking (DMA-paced) and uses short
time slices.

---

## 26. Repository layout (final)

```
G:\LED project\
├── README.md                  overview, quickstart, wiring summary, safety
├── docs\                      ARCHITECTURE, HARDWARE, BOM, WEB_DASHBOARD,
│                              TESTING, ROADMAP, POWER (worked calcs)
├── firmware\                  PlatformIO project (C++/Arduino, esp32-s3)
│   ├── platformio.ini
│   ├── src\
│   │   ├── main.cpp
│   │   ├── util\               Rgb/HSL/palette helper, Smoother, Log
│   │   ├── audio\              AudioSource|I2SMicSource|AudioAnalyzer|BeatDetector|TestToneSource|SourceSelector|SourceKind
│   │   ├── device\             DeviceTypes|DeviceProfile|DeviceRegistry|DeviceManager (discovery + /devices.json)|DeviceInsight (confidence/integrations)|DiscoverProtocol (pure wire codec)
│   │   ├── theme\              ThemeEngine + ThemeBlend (cross-fade)
│   │   ├── led\                LEDDriver|addressable\ |analog\ |single\ |factory\
│   │   ├── effects\            Effect|LedFrame|EffectParams|EffectRegistry|fx\…
│   │   ├── config\             ConfigDefs|Config|ConfigStore|ConfigDefaults
│   │   ├── network\            (Ph4) wifi, AP/STA, REST api, SSE stream
│   │   ├── system\             (Ph4+) diag, memory/cpu, ota
│   │   └── runtime\            StripRuntime, App, AudioFrame pub/sub
│   ├── data\                   LittleFS: web assets + default config.json
│   └── test\                   host unit tests (x86 target)
├── web\                        TypeScript + React (Vite)
│   ├── src\                    App.tsx (single-page dashboard) + index.css
│   ├── mockDevPlugin.ts        Vite dev-server mock of the firmware /api surface
│   └── dist\                   build output → firmware/data
├── tools\python\               audio-analysis lab, simulators, power_calculator
└── hardware\                   (later) PCB, enclosures, schematics
```

This preserves the exact separation of responsibilities requested, in the
project's own layout conventions (PlatformIO for firmware, Vite for web, Python
for dev tools).