# Development Roadmap

Incremental, per the spec's phase ordering. A **✓** means the code is in the
repo; a phase is *done* when its acceptance criteria pass on hardware.

| Phase | Scope | Key deliverables | Acceptance criterion | Status |
|---|---|---|---|---|
| **1** | Audio + one addressable strip | `AudioSource`/I2S INMP441, FFT, configurable bands, dB norm, smoothing, basic beat, `AddressableLEDDriver` (WS2812+), one Spectrum/Bass response working on a 1–2 m strip | music from a phone → stable, clean visual response on 30–60 LEDs | **buildable, to bench-test** |
| **2** | Effects engine | `Effect` interface + `LedFrame`, all 12 effects (Spectrum, Bass Pulse, Beat Flash, Freq Wave, Freq→Color, Rainbow Music, VU, Energy Pulse, Running Wave, Spark, Bass→Treble Gradient, Custom Mapping) | all effects render correctly to the frame layer (host tests + visual) | engine + effects in repo (host-testable) |
| **3** | Configuration & presets | JSON on LittleFS, versioned schema + migration, defaults, presets CRUD | config survives reboot; old config migrated across versions | basic store in repo |
| **4** | Wi-Fi + web dashboard | AP + STA, REST + SSE, `web/` (Vite + React + TS) SPA with all sections, admin token, live stream + preview | phone configures everything over Wi-Fi, no cloud | **code in repo (needs bench)** |
| **5** | Multiple addressable strips | per-strip config independence (driver, effect, mapping, pins); RMT first, LCD-CAM parallel (16) if needed | 2–4 independent WS281x strips w/ different effects | planned |
| **6** | Conventional RGB strips | `AnalogRGBDriver` (LEDC PWM, common anode/cathode), MOSFET stage, zone→band | same engine drives 5 V RGB zones (bass/mid/treble test §37) | drivers in repo, needs bench |
| **7** | Single-colour strips | `SingleColorDriver`, PWM brightness | mono strip pulses to beat/bass | drivers in repo |
| **8** | Additional protocols + OTA | WS2811/WS2815/SK6812-RGBW/APA102/HD107 verified; dual-partition OTA + rollback | each chipset demo; OTA upgrade/rollback test | chipsets in driver; **OTA in repo (HTTP + ArduinoOTA), needs bench** |
| **8b** | Art-Net DMX output | `ArtNetNode` (WiFi STA, UDP, ArtPoll/ArtDmx), fixture profiles (moving head / LED par), audio→DMX mapping | moving heads swing to the beat over WiFi | **code in repo, needs bench** |
| **8c** | Theme engine | data-driven `ThemeEngine` (Audio→Theme→Effect pipeline), **18 themes** incl. distinct Afro House + Auto classifier (hysteresis + dwell), per-strip theme/effect overrides, `/api/state*` unified live state | themes switch cleanly, dashboard + touchscreen show the same active state | **code in repo, builds green** |
| **8d** | Touchscreen UI | LVGL on hardware-independent `DisplayDriver`/`TouchDriver` (`DisplayManager`), ili9488 + FT6236 drivers, Now/Themes/System screens, `esp32-s3-ui` env | panel theme/brightness control matches the web dashboard | **code in repo, builds green** |
| **9** | Multi-controller | UDP-multicast `AudioFrame`, master/slave roles, clock offset, frame counter | 2+ controllers render synchronised to one master | **code in repo (SyncNode + SyncClock), builds green** |
| **10** | Productisation | carrier/HAT PCB (2-layer splice), commodity enclosure, connectors/protection/power-dist, user-manual outline, CE self-cert + RoHS technical file | design-docs + BOM for production (`docs/PRODUCTISATION.md`, `docs/BOM.md`) | **design doc + BOM in repo** |
| **11** | System controls | graceful restart + safe power-off (deep sleep, optional wake pin), `/api/system/*` + live `sensitivity`/`backlight`/`timeout` endpoints (no reboot), web System tab + LVGL confirmations, output-safety shutdown sequence | live tuning never triggers a reboot; restart/power-off silence all outputs first (see `docs/SYSTEM.md`) | **implemented, builds green, bench pending** |
| **12** | Universal device detection & insights | capability registry + RESO_DISCOVER codec (`DiscoverProtocol.h`), tri-layer confidence + integration hints (`DeviceInsight.h`), `capCatalog`/`connCatalog`, manual identify (`declare()`), audio-source availability derived from trusted devices + boot `active`/`reason`, Devices & Sources tab (Details/manual forms) with mock parity | 84 host tests green; native + esp32-s3 + esp32-s3-ui + web builds green; detection never drives outputs; re-scan cannot downgrade a trusted row | **implemented, builds green, bench pending** |
| **13** | Cinematic Mode (movie/TV scene-reactive lighting) | pure `SceneAnalyzer`/`CinematicEngine`/`SceneFrame` codec + `cinema/` headers, 5 reaction presets + 2 genre overlays, **bounded scene-memory ring + 8-mood intent director** (friendly labels), **optional spatial `SceneFrame` block + wave-propagation room mapping** (`SpatialBlock.h`/`SpatialWaveField.h`, per-strip `roomX/roomY`, wave knobs), **multi-source companion picker with skew estimate**, **comfort tiers + audio/video sync offset + link-latency/jitter status**, **test/demo injector + QA bursts** (`/api/cinematic/test`, web QA card, touchscreen QA button, looping demo script), **debounced config flash-save**, `SceneLinkNode` UDP-multicast companion receiver, engine→theme applier, `/api/cinematic` GET/POST, Cinematic web tab + LVGL Cine screen, Python reference companion (`tools/companion/`) | 144 host tests green; native + esp32-s3 + esp32-s3-ui + web typecheck/build green; companion feed never drives outputs directly (engine → theme → LEDDriver only); silent companion falls back to on-device audio; sustained loud noise never strobes; mapping off ⇒ prior behavior byte-for-byte | **implemented, builds green, bench pending** |

## Windows Control Center (desktop app)

Separate lane for the Windows desktop application built on top of the
firmware platform (`docs/DESKTOP.md` for architecture + framework decision).
Phases below are D-phases (desktop); they do not block or unblock firmware
phases. Definition-of-done per phase: code + host tests green + README/docs
updated in the same commit.

| Phase | Scope | Status |
|---|---|---|
| **D1** | Architecture, framework choice (Electron), client layer (REST + UDP discovery), honest simulator, app shell with Home + Setup, IPC contract, smoke boot | **in repo, builds + 14 host tests green, smoke boot verified** |
| **D2** | First-run setup wizard (highest priority): guided find/verify/rename/Wi-Fi hand-off, byte-exact config backup before writes, reboot-aware reconnect, honest errors | **in repo, builds + 21 host tests green, smoke verified** |
| **D3** | Main Control Center: Lights (by name, per strip), Music (plain-language sources + auto-select), Themes (swatches, global + per strip), Cinematic (engine toggle, scene readout, QA bursts) | **in repo, builds + 40 host tests green, smoke verified** |
| **D4** | Diagnostics + system controls: guided hardware check (live, honest step-by-step: reachability, health, configured strips, live audio signal, brightness write round-trip with a visible blip, then a human confirm), health; session event log (in-memory, secret-free), structured report export (no secrets/addresses), restart/power-off behind confirms | **in repo, builds + 53 host tests green** |
| **D5** | Firmware & recovery: `/api/ota` update, backup/restore, esptool flash recovery | planned |
| **D6** | Polish + installer: electron-builder NSIS installer (single-file, one-click, desktop shortcut), full **brand identity + logo system** (`docs/BRAND.md`, `desktop/build/`) | **installer + logo system in repo; auto-update, tray, UX audit pending** |

Hardware bench remains the single blocker for every firmware phase marked
"bench pending" (`docs/BENCH.md`); the desktop lane is host-only until then
and is verified against the in-process simulator + smoke boot.

## Near-term plan (after architecture sign-off)

1. Phase-1 bench bring-up: build firmware → flash S3 → serial VU/band stats →
   WS2812B 60 LEDs → verify Spectrum + Bass Pulse against a phone playlist.
   Ordered checklist in `docs/BENCH.md` (all phases covered, incl. Ph 9 sync
   and Ph 10 carrier hand-off).
2. While hardware is on order: host-test effects + Python lab cross-validation.
3. Then Phase 2 polish → Phase 3 → Phase 4 (the biggest single software chunk).

## Ownership of risk

- Blocking risk: IDF-5 I2S/PSRAM bug on S3 → we're pinned to Arduino core 2.0.x
  (legacy I2S, the proven INMP441 path). Contained behind `AudioSource`.
- FastLED on S3 = RMT; parallel strips (Ph 5/9) may need the LCD-CAM driver —
  documented upgrade path.
- Web bundle size: guarded by Vite budget; fallback trim if embedding is tight.