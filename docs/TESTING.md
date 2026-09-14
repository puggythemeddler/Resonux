# Testing Strategy

Four levels, cheapest first. The DSP + effects layers are written so they can
build and run on a host (x86) with no Arduino headers, which is what makes
meaningful automated tests cheap.

## 1. Host unit tests (x86, CI-friendly)

**Implemented**: `pio test -e native` runs 144 Unity assertions across 5 host
suites — `test_logic` (colour math, smoothing, effects), `test_sync`
(multicast clock/packet), `test_system` (restart/power-off state machine),
`test_device` (universal detection: capability model, device registry,
profile validation incl. the AM-006 conditioning profile, deterministic
`classify()`, hysteresis-aware `SourceSelector` fallback/preferred logic,
`TestToneSource`, theme cross-fade blending, the RESO_DISCOVER wire codec
round-trip/garbage/bounds, tri-layer confidence scoring, integration
recommendation mapping, `deviceEnablesSource`, and `manualDeclareStatus`) and
`test_cinema` (Cinematic Mode: SceneFrame codec round-trip/validation/idents,
**spatial-block codec** round-trip/over-long/malformed/zone idents, **spatial
wave field** rest/peak/propagation/ring-bound/line-sweep, config presets +
network-field preservation + clamp/NaN handling, `SceneAnalyzer`
boom/impact cooldowns + sustained-loud/whisper/silence/dialogue/tension gates,
`CinematicEngine` audio-only/video/fused fusion, flash envelope + min-gap, boom
re-trigger blocking, failsafe/stale, whisper-dim, video colour tint, genre
overlays, **scene-memory/director** label+mood cascades/holds/energy, **wave
spawns** on boom/change, `applyToThemeFrame` maths incl. zone-scale parity,
**companion picker** (single-source/switch/replay/wrap/tie-break/evict/skew/
switch-back), **comfort tiers + sync-offset envelope lead/delay + link
latency/jitter estimation**, and **TestInjector** entry-dwell/loop-wrap/spatial
sample) — against the pure layers:
`util/Rgb.h` (HSL↔RGB, palettes, blend/scale/luma/clamps), `util/Smoother.h`,
`effects/EffectUtil.h`, `effects/LedFrame.h`, `audio/AudioFrame.h`, real
effects (`BassPulseEffect`, `GradientEffect`), `firmware/src/system/SystemMode.h`,
`firmware/src/device/*`, `firmware/src/audio/SourceSelector.h` and
`firmware/src/cinema/*`
(`SceneFrame.h`, `SpatialBlock.h`, `SpatialWaveField.h`, `SceneMemory.h`,
`CinematicDirector.h`, `CinematicConfig.h`, `SceneAnalyzer.h`,
`CinematicEngine.h`, `CinematicApply.h`, `CompanionPicker.h`,
`TestInjector.h`).
Note: `pio test -e native` compiles test sources only, which is why the new
device/source/blend logic lives in Arduino-free headers (the registry, profile
table and selector run unchanged on host and device).

Planned extensions: `BeatDetector`, every remaining `Effect` (render into an
in-memory `LedFrame`, sample a few pixels), and DSP band aggregation. `LedFrame`
and the effect interfaces stay pure — effects never touch `LEDDriver` or
FastLED (already enforced by design, verified at compile).

## 1b. Control Center host tests (`desktop/`, vitest)

`cd desktop && npm test` runs 40 tests across 5 suites (no hardware, no
network peers):

- `discovery.test` — the TypeScript RESO_DISCOVER codec port round-trips the
  firmware wire format, tolerates unknown keys, and rejects non-responder
  datagrams (keeps the desktop scanner byte-compatible with the C++ codec).
- `mock.test` — the `MockController` simulator serves firmware-identical JSON
  (`/api/status`, `/api/cinematic`, state endpoints), mutates brightness with
  0–255 validation, honours the cinematic toggle, 404s on unknown routes;
  plus **config writes**: `PUT /api/config` renames the device, the AP-mode
  variant performs the Wi-Fi hand-off (ap → sta, honest `rebooting:false`),
  and a keep-port `restart()` preserves address + state; plus **the D3
  surface**: themes list/global + per-strip select/PUT + DELETE/reset, a
  per-strip effect endpoint with validation, audio source pick + auto-select
  (both reflected in `/api/audio/sources`), and the cinematic QA burst.
- `registry.test` — a registered controller heals online and reports latency;
  a controller that stops answering goes offline and **loses its stale status
  stats** (identity survives); a restart brings it back online on the same
  address (the config-reboot path); removal works.
- `app.test` — the full `AppCore` boots, self-starts the simulator, selects it,
  and round-trips live commands; **rename/setWifi write through config with a
  byte-exact backup left in `userData/backups`**; the first-run
  `wizardNeeded` lifecycle (pick a controller or finish the wizard);
  `waitOnline` answers for the running controller and times out honestly for
  an unknown one; and the **D3 commands** round-trip into the running
  simulator: `getState`, `getThemes`, `selectTheme` (global + per-strip),
  `setStripEffect`, `selectAudioSource`, `setAutoSelect`, and
  `triggerCinematicTest`.
- `check.test` — the **guided hardware check engine** (D4): `blipTarget` never
  picks 0 and never blasts full brightness; a healthy, live controller
  (varying amp) passes every step and restores brightness; an unanswerable
  controller fails fast; a flat audio signal and a brightness write that
  won't return home are honest **warns** rather than failures; and the full
  `AppCore.runHardwareCheck` passes every step against the in-process
  simulator while explicitly labelling it *Simulator*.

Command parity is the contract: the simulator is deliberately kept in lockstep
with the firmware REST surface so a desktop surface that works in tests works
on a real controller at bench bring-up (`docs/BENCH.md`).

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
| Control Center | codec/registry/simulator parity (vitest) | – | – | REST contract vs MockController + firmware |
| Presets | CRUD | – | – | persist after reboot |
| Power calc | python util unit | worked examples | – | – |
| Error handling | rejects in config loader | – | diag page | – |