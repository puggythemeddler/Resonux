# Testing Strategy

Four levels, cheapest first. The DSP + effects layers are written so they can
build and run on a host (x86) with no Arduino headers, which is what makes
meaningful automated tests cheap.

## 1. Host unit tests (x86, CI-friendly)

**Implemented**: `pio test -e native` runs 69 Unity assertions across 4 host
suites — `test_logic` (colour math, smoothing, effects), `test_sync`
(multicast clock/packet), `test_system` (restart/power-off state machine),
`test_device` (universal detection: capability model, device registry,
profile validation incl. the AM-006 conditioning profile, deterministic
`classify()`, hysteresis-aware `SourceSelector` fallback/preferred logic,
`TestToneSource`, and theme cross-fade blending) —
against the pure layers: `util/Rgb.h` (HSL↔RGB, palettes, blend/scale/
luma/clamps), `util/Smoother.h`, `effects/EffectUtil.h`, `effects/LedFrame.h`,
`audio/AudioFrame.h`, real effects (`BassPulseEffect`, `GradientEffect`),
`firmware/src/system/SystemMode.h`, `firmware/src/device/*` and
`firmware/src/audio/SourceSelector.h`.
Note: `pio test -e native` compiles test sources only, which is why the new
device/source/blend logic lives in Arduino-free headers (the registry, profile
table and selector run unchanged on host and device).

Planned extensions: `BeatDetector`, every remaining `Effect` (render into an
in-memory `LedFrame`, sample a few pixels), and DSP band aggregation. `LedFrame`
and the effect interfaces stay pure — effects never touch `LEDDriver` or
FastLED (already enforced by design, verified at compile).

## 2. Python audio-analysis lab (`tools/python/`)

Cross-validates the algorithm design before it gets ported/optimised, and
produces the spec's visualisations (§40):

- load WAV/MP3 → replicate band & beat pipeline with NumPy (independent
  implementation of the same maths) → emit:
  - waveform
  - spectrum + band overlays
  - band-level timelines
  - beat markers over the waveform
  - energy/amplitude trace
  - **simulated LED output** (strip renderer; save GIF/PNG walk-through)
- Used to tune band limits, dB mapping, thresholds and smoothing against real
  songs; the firmware DSP is then validated against the same stimulus file.

## 3. Hardware test mode (on-device, spec §22/§31)

- `POST /api/test`: red / green / blue / white / full / single-LED / zone /
  strip sweeps — for troubleshooting wiring with eyes, not logic.
- Diagnostics (`GET /api/diag` + System page): mic present?, live audio level,
  FFT ok, LED driver initialised, Wi-Fi state, memory, CPU load, config
  version, last errors.
- Serial console equivalent for bench work: `diag` command prints the same.

## 4. Firmware + web integration tests

- Config CRUD round-trips (save → reboot → load → byte-identical semantics),
  preset persistence, pin-validation rejections, OTA guarded paths (Ph 8).
- Simulated audio source (`AudioSource` subclass emitting generated PCM)
  drives the full pipeline into a null `LEDDriver` — lets us assert effects
  produce expected pixel values end-to-end without hardware.
- Web API contract tests (fetch-level) against a running firmware instance;
  SSE stream sanity (heartbeat, monotonic counters).

## Test matrix → spec §39

| Area | Level 1 | Level 2 | Level 3 | Level 4 |
|---|---|---|---|---|
| Audio input | – | waveforms | mic level/FFT ok | sim source through pipeline |
| FFT / bands | band aggregation | band timelines | – | – |
| Beat detection | synthetic beats | beat markers | – | – |
| Effects | pixel-value tests | LED sim video | visual on strip | – |
| LED abstraction | null-driver tests | – | – | – |
| Driver (addressable) | – | – | test mode | – |
| Driver (analog/single) | duty assertions | – | test mode Zones | – |
| Config | round-trip | – | – | API tests |
| Web API | – | – | – | contract tests |
| Presets | CRUD | – | – | persist after reboot |
| Power calc | python util unit | worked examples | – | – |
| Error handling | rejects in config loader | – | diag page | – |