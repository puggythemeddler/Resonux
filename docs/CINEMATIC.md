# Cinematic Mode — Movie/TV Scene-Reactive Lighting

Status: **implemented, builds green** (both firmware envs, web typecheck/build,
144 host tests). Bench pending (hardware not yet arrived).

## Why

The theme engine makes lights follow *music*. Watching a movie or TV show, the
"beat" is gone and the audio is sparse — what a lighting controller has instead
is *content*: quiet dialogue, a sudden explosion, a chase, a dark scene. The
ESP32's own microphone hears the whole room plus the TV speaker, so on its own it
can only guess.

Cinematic Mode solves this with a small division of labour:

- A **companion** (a PC app, `tools/companion/`) watches the *program source*
  on the host and streams a tiny binary **SceneFrame**: scene kind, discrete
  events, average luminance, dominant colour, motion, observed program-audio
  level.
- The **ESP32** fuses that feed with its own on-board audio analysis and
  *multiplies* a subtle look onto whatever theme is playing. The companion never
  drives LEDs directly — the engine only ever feeds the Theme Engine, which
  already owns the strips.

Everything is built to fail safe: companion silent → pure on-device audio after
`staleMs`; sustained loud content can't strobe (cooldown-gated envelopes).

## Data flow

```
 companion (PC)                     ESP32
 ─────────────────               ────────────────────────────
 program audio ─► HostAudio     SceneLinkNode ◄─ UDP 239.255.42.11:9772
 screen region ─► HostVideo     ──► exposeCameraFeed (mutex copy)
        │ pack_frame()                │
        ▼                             ▼  stale>staleMs → drop
 SceneFrame (28 B) ────────► CinematicEngine.update(audioFeatures, scene?)
        └─ optional spatial            │  audio features ◄─ SceneAnalyzer
           block: focus +              ▼                   (on-device)
           zone salience        Look {brightness, hueShift, flash, …}
        (+≤21 B, drives the           │  (+ per-strip wave zoneScale,
         room-mapping wave)           ▼   room mapping)
                              applyToThemeFrame() × every strip
                                     │
                                     ▼
                             ThemeEngine → Effects → LEDDriver   (existing path)
```

The engine emits a *Look* that is merged with the theme frame by
`applyToThemeFrame` (brightness scale, saturation hue-shift, beat flash +
sparkle bump, transition-speed slowdown, tint on the theme palette). Detection
≠ activation: audio analysis and video blending live in separate headers so each
can be tuned and host-tested independently.

> `src/cinema/` is not linked to the LED path directly. `App::ledLoop()` calls
> `cinematic` only when `cinematic.enabled` — otherwise the exact same loop
> runs as before. (Phrased plainly: unless you switch it on, this feature costs
> nothing and changes nothing.)

## Wire protocol (`cinema/SceneFrame.h`)

Packed 28-byte base packet, little-endian, UDP multicast `239.255.42.11:9772`
(magic `0x53434E52` = 'RNCS', version 1):

| Field | Bytes | Meaning |
|---|---|---|
| magic / version / flags | 4 / 2 / 2 | 'RNCS', 1; bit 13 = spatial block follows |
| seq | 4 | companion frame counter (dedupe) |
| hostTimeMs | 4 | companion clock (rate/staleness probe) |
| sceneId | 1 | SceneKind: undefined, speech, quiet, action, chase, explosion, music |
| eventId | 1 | SceneEvent: none, whisper, flash, boom, dark, change |
| eventConfidence | 1 | 0..100 |
| avgLuminance | 1 | 0..255 scene average |
| hue / sat / val | 3×1 | dominant colour (0..255 → 0..360°) |
| motion | 1 | 0..255 motion energy |
| progAudio | 1 | 0..255 companion-observed program audio |
| sourceFlags | 1 | bit0 program-audio observed, bit1 pillarboxed |
| pad | 2 | zero |

`validFrame()` rejects short/corrupt payloads, wrong magic/version, or
out-of-range ids but **tolerates over-length payloads**, so receivers that
predate the spatial block simply ignore its trailing bytes. `SceneLinkNode`
(receive-only, core 0 task, mutex-guarded latest-frame copy) is the ESP32 side;
`tools/companion/resonux_companion.py` (pure stdlib codec, heavy deps loaded
lazily) is the reference transmitter.

### Companion source identity (`cinema/CompanionPicker.h`)

The receiver keys frames by UDP source (ip:port) so competing companions don't
fight the scene line. One **active** source owns the feed; up to **4** sources
are tracked in total. Rules:

- Sequence guard `(int32_t)(seq - lastSeq) > 0` drops replayed/out-of-order
  frames; wrap-aware counts remain valid across the 32-bit boundary.
- Live non-active sources are *tracked* (counted, last-rx updated) but their
  frames return `REJECT_INACTIVE` — never committed while the active source is
  streaming.
- If the active source goes stale, the picker switches to the best alive
  candidate (vote count desc → last-rx desc → ip asc → slot asc) and only then
  commits that sender's frame; a stale sender that fires *now* re-arms instead.
- A full table evicts the deterministic weakest **non-active** slot; the active
  slot can never be evicted.
- A smoothed **skew estimate** (ppm) is seeded on activation (~1 s) and updated
  with the connection so the web panel can show how far a companion's clock
  drifts.

`sourceLabel()`, `sourceCount()`, `skewPpm()` and `lastRxMs()` are surfaced in
`GET /api/cinematic` (see API).

### Spatial block (optional)

A companion with per-pixel screen data appends a length-tagged **spatial block**
after the 28-byte base and sets bit 13 (`SFLAG_SPATIAL_BLOCK`) in `flags`.
Layout (`cinema/SpatialBlock.h`):

```
'S' 'P' blockLen focusX focusY  (zoneId confidence) …
```

`blockLen` = `2 + 2·N` (N zone pairs, 2..18), so the whole block is at most
`3 + 2 + 2·8 = 21` bytes. `focusX/focusY` (0..255, 128 = unknown) is the
centre of visual mass on screen; each pair is a `ZoneId` + salience confidence
(0..100). Zone ids: left / centre / right / top / bottom / the four corners /
full-screen.

The block is pure geometry — no pixels — so even a low-power receiver can
steer a room-mapping wave without knowing the scene. A malformed block
(bad magic, odd/oversized length, unknown zone) is rejected and the receiver
keeps its last good spatial sample.

## On-device analysis (`cinema/SceneAnalyzer.h`)

Pure features over the FFT bands (`AudioFeatures`): level, bass/mid/treble,
silence, whisper, dialogue, sustained-loud, beat/beatStrength, boom, impact,
tension. Headlines:

- **Boom**: bass rise > 0.30 vs a *slow* baseline, blocked while sustained-loud,
  450 ms cooldown. Sustained loud music builds the baseline, so it can never
  spam booms.
- **Whisper** 0.03..0.12 level; **silence** < 0.03; **tension** rises on
  mid/treble creep at moderate-low level and collapses when it gets loud.

## Engine (`cinema/CinematicEngine.h`, `CinematicApply.h`)

`update(caf, scene?, now)` per LED frame with `startTimeMs`-based envelope
holds+decays. Sources: `SRC_LOCAL_AUDIO` (no/lost feed), `SRC_VIDEO` (video
primary, audio cross-check), `SRC_FUSED` (both live). Events hold a moment then
decay; video fields are smoothed to the LED frame rate. `buildLook()` applies
scene-aware tint tweaks (explosion→orange, chase→red/orange, music→yellow,
whisper→cool+dim, tension→red creep), flash rides to `maxBrightness` (slightly
desaturated), boom dips, ambient floor from scene luminance, genre overlays.
`applyToThemeFrame()` mirrors those values onto the active theme (beat flash
`+ flash*0.55`, sparkle `+ flash*0.25`, intensity `× (1-0.5·calm)`,
transition speed `× (1-0.6·smoothing)`, tint on primary/accent/secondary/
background by `tintMix·0.7/0.45/0.25`).

### Comfort tiers

A global comfort level scales the punchy stuff without touching the tuning
sliders, so "Emotive" presets stay usable after dark:

| Comfort | Flash ceiling | Gap multiplier | Brightness ceiling |
|---|---|---|---|
| **Film** | 0.5× | 2.2× | 0.72× |
| **Standard** | 1.0× | 1.0× | 1.0× |
| **Vivid** | 1.2× | 0.8× | 1.0× |
| **Extreme** | 1.5× | 0.55× | 1.0× |

`comfortFlashScale` scales the flash top, `comfortGapScale` lengthens/shortens
the flash/boom cooldown gaps, `comfortBrightnessScale` caps `maxBrightness`.
Cooldown tracking is hold-based with a `0xFFFFFFFF` sentinel ("never fired")
so a freshly armed comfort switch can't produce instant strobes.

### Audio/video sync offset & link latency

`syncOffsetMs` (−2000..2000, default 0) shifts the *scheduled start* of every
envelope: event starts are computed as `(int64_t)nowMs + syncOffsetMs`, so a
positive value parks bursts until the video catches the audio that triggered
them (and a negative value leads). The engine reports `status.linkLatencyMs`
and `status.jitterMs` — the smoothed half of the host-arrival diff measured on
the receiver (`est=(dl−dh)/2` while `0 ≤ dh ≤ dl`) — so the dashboard can show
exactly how far the companion feed trails the wall clock.

## Scene memory & intent (bounded)

The raw instantaneous feed is noisy (scene = speech for a sentence, then chase
for a cut). A bounded **scene-memory ring** (`cinema/SceneMemory.h`) smooths it:
per-kind agreement votes switch the running scene only after repeated
consensus (hysteresis — except an explicit scene `change` event, which jumps
the line), a smoothed **mood energy** 0..1 rises fast on events and decays
slowly, and per-kind dwell/transition timers keep the whole thing stable during
rapid cuts. The **director** (`cinema/CinematicDirector.h`) turns that memory
into an 8-mood intent (calm → suspense → tension → action → impact → aftermath
→ transition → performance) with a deterministic cascade, short hold timers to
stop flicker (holds re-arm on actual change; impact punches through), and
genre-aware tuning, producing `CinematicIntent {mood, energy, tintWarmth,
flashScale, pulseDepth, …}` that the engine consumes. Moods have friendly
labels surfaced in the web and touchscreen UIs.

## Room mapping (spatial wave propagation)

With the companion watching a screen region, the ESP32 can place light *where*
the action is — without any per-zone geometry of its own. Knobs on
`/api/cinematic` (`roomMapping` is off by default):

| Knob | Default | Range | Meaning |
|---|---|---|---|
| roomMapping | off | bool | master switch (off ⇒ byte-for-byte legacy behavior) |
| waveSpeed | 2.0 | 0.5–5 | screen-widths/sec a wave-front travels |
| waveDecay | 0.8 | 0.1–2 | how fast a passing wave fades |
| waveWidth | 0.7 | 0.1–1 | wave bell width (screen-relative) |
| maxWaves | 8 | 4–12 | bounded wave ring depth |

Each strip also gains `roomX`/`roomY` (0..1, screen coordinates, in
Configuration). When mapping is on, the engine spawns a wave at the current
spatial focus on impactful events — cooldown-gated booms/flashes (point wave),
scene **changes** as a directional sweep from the previous focus, motion
**chases** re-spawning every ~400 ms, and local-audio booms at the last known
focus (screen centre when none) — and per strip computes

```
zoneScale = 0.70 + 0.30 × SpatialWaveField.intensityAt(roomX, roomY, now)
```

multiplied onto that strip's theme brightness. The wave field is a bounded ring
of point/line waves in pure `SpatialWaveField.h` — old waves evicted, rest ⇒ 0,
intensity clamped, so a dead or frozen feed yields zero zone energy and the
lights simply rest on the ambient floor (never stutter). `status.spatialActive`
tells the UI whether mapping has a live spatial feed.

## Presets & config

Five reaction presets (`cine::Mode`): Subtle / **Balanced (default)** /
Immersive / Dynamic / Extreme — `applyPreset()` fills reaction, influences,
speed, smoothing, flash intensity + timings while **preserving** the network
fields (group/port/stale) so picking a preset never loses the companion port.
Two genre overlays (Horror → dim + desaturate; Anime → brighten + pulse the
dominant colour) sit on top of a mode, and a **comfort tier** (`c.comfort`,
Film/Standard/Vivid/Extreme) scales flash, gaps and brightness ceiling on top of
both (see above). `clampConfig()` bounds every knob
(NaN → low bound) so `POST /api/cinematic` can't write a harmful value
(`reaction` to 1.5 by design — Extreme runs hot at 1.25). The room-mapping
knobs above clamp the same way; strip `roomX/roomY` (0..1) live in the main
`Config` and are persisted with it.

## Test & demo mode (`cinema/TestInjector.h`)

No companion in the room? The controller can drive the *whole* pipeline itself.
`TestInjector` is a pure synthetic SceneFrame source: a scripted sequence of
entries (scene/event/confidence/dominant colour/motion/wave focus) is packed
into real `SceneFrame`s with a monotonic seq and a loopback `hostTimeMs` — the
engine sees a perfectly fresh, zero-latency feed.

- **QA bursts** (`POST /api/cinematic/test`, or the web Cinema tab's *QA test
  burst* card, or the touchscreen Cine tab's *QA burst* button) inject one
  entry for a set dwell (`lengthMs`, default 800 ms). The engine reverts to the
  live feed / device audio the instant the burst is spent.
- **Demo loop** (`cinema.demo` toggle): a built-in ~10.5 s 9-entry script
  (action→flash→explosion boom→chase cut→…→speech) runs on loop so comfort,
  sync offset and room-mapping waves can be eyeballed on any content. Real UDP
  frames and one-shot bursts always take priority and the loop resumes after.
- Apps and bursts are mutually safe: a burst never touches the persisted
  config; the loop doesn't either — both are runtime-only.

## Failsafes

1. Companion silent `> staleMs` (default 1200 ms) → `SRC_LOCAL_AUDIO`, lights
   keep reacting to the device mic. `companionAlive` and `source` are exposed in
   `/api/cinematic` so the dashboard always says which feed it's on.
2. `> staleMs × 4` → scene cleared to Undefined (ambient neutral).
3. Sustained-loud music → boom gate; flash envelope bounded by
   `flashMinGapMs` (90 ms default, hard no-strobe cap).
4. Everything the engine emits is *multiplied* onto the theme frame and capped
   by `maxBrightness` and the theme's own brightness — a black screen or dead
   companion can never command full white.
5. Spatial safety: a malformed spatial block is dropped (last good sample
   kept); a frozen screen yields zero-confidence zones so room mapping rests on
   the ambient floor instead of strobing; and the whole mapping is opt-in
   (`roomMapping` off = previous behavior exactly).

## API

| Method & path | Body | Effect |
|---|---|---|
| `GET /api/cinematic` | — | `{config, active, companionAlive, companionSource{Label,Count,SkewPpm,LastRxMs}, demoActive, status{source, scene, event ids/labels, confidence, luminance, motion, progAudio, hue, sat, val, audioLevel, boom, tension, lastFrameMs, linkLatencyMs, jitterMs, mood, moodEnergy, recentEvents, spatialActive}}` |
| `POST /api/cinematic` | partial merge | Apply optional `{applyPreset:true, mode}` then merge scalar fields (`comfort`, `syncOffsetMs`, `demo` included); live-applied through `App::setCinematicConfig` (engine reconfigured, `SceneLinkNode` recreated, flash save debounced ~500 ms after the last change — no reboot) |
| `POST /api/cinematic/test` | `{scene, event, confidence, lengthMs, focusX, focusY}` | One-shot QA burst — a synthetic SceneFrame injected for `lengthMs`, then the engine reverts to the live feed / device audio |

Web **Cinematic** tab (toggle, presets, genre, comfort, sync offset, demo loop,
sliders, companion link, live status + latency, QA burst) and the LVGL **Cine**
screen (toggle + preset cycle + QA burst + status) share the same
`Config`/`Status` source of truth. Companion status, presets and knobs all
round-trip live without a reboot; debounced saves batch rapid slider drags into
one flash write.

## Safety / honesty notes

- `CAP_SCREEN_CAPTURE/VIDEO_INPUT/HDMI_CAPTURE/SYSTEM_AUDIO` (capability bits
  19–22) and `CONN_SCENE_UDP` are **capability/connection channels** in the
  device registry — they describe what an input device could offer, they never
  imply a driver exists. The ESP32 only *receives* SceneFrames; it has no HDMI
  or screen-capture capability of its own.
- The reference companion is experimental: it will capture what you point it at
  (audio device, screen region). Use it only with content you may observe;
  default it to nothing and opt in per run.
- `tools/companion/` is a reference implementation, not a shipped product —
  lazy imports and `--sim` keep it runnable with zero third-party deps.