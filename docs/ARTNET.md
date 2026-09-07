# Art-Net DMX Output

The controller can drive **DMX fixtures** (club-style moving heads, LED pars,
strobes) by broadcasting **Art-Net** over Wi-Fi — the same UDP packets a
lighting console sends on port 6454. No DMX transceiver, cable, or interface
board is required; any fixture with an Art-Net node (or a wired DMX universe
fed by an Art-Net-to-DMX gateway) will respond.

## When to use it

- You already have moving heads / pars that speak **DMX512** and a **wired or
  wireless Art-Net gateway**.
- LEDs *and* fixtures react to the same music: the identical `AudioFrame`
  feeds both the LED effects and the DMX mapping.
- No extra hardware on the ESP32 side.

## How it works

```
ESP32-S3 ──Wi-Fi──► Art-Net (UDP 6454, broadcast) ──► DMX gateway ──► fixtures
```

- One **512-channel** DMX universe (`artnet.universe`, 0–255).
- The node joins your Wi-Fi (DHCP or static IP), reconnects automatically.
- Broadcasts the universe at ~40 Hz.
- Answers **ArtPoll**, so consoles / `artnetpoll` discover it as
  `Resonux Art-Net Node`.
- Can also **receive** ArtDmx from a console and pass it through.

## Configuration

Edit `firmware/data/config.json`, then flash the filesystem
(`pio run -t uploadfs`). Art-Net is **off by default**.

```json
"artnet": {
  "enabled": true,
  "ssid": "your-network",
  "password": "your-password",
  "useDhcp": true,
  "staticIp": [0, 0, 0, 0],
  "staticMask": [255, 255, 255, 0],
  "staticGw": [0, 0, 0, 0],
  "universe": 0,
  "audioReactive": true,
  "panSpeed": 0.5,
  "tiltSpeed": 0.5,
  "colorSensitivity": 1.0
}
```

| Field | Meaning |
|---|---|
| `enabled` | master switch |
| `ssid` / `password` | Wi-Fi to join (STA only) |
| `useDhcp` | `true` = DHCP; `false` = use `staticIp`/`staticMask`/`staticGw` |
| `universe` | Art-Net universe (0–255), matches the gateway's selection |
| `audioReactive` | let the audio engine drive the DMX output |
| `panSpeed` / `tiltSpeed` | 0–1 swing strength for moving-head pan/tilt |
| `colorSensitivity` | scales RGB/dimmer response |

## Fixtures

Each fixture entry assigns a **profile**, a **start address**, and a
**count** of identical units. Consecutive units auto-step by the profile's
channel count.

```json
"fixtures": [
  { "profileId": 1, "dmxAddress": 1,  "count": 2 },
  { "profileId": 3, "dmxAddress": 33, "count": 4 }
]
```

### Built-in profiles

| ID | Name | Channels | Layout |
|---|---|---|---|
| `0` | Off | — | unused |
| `1` | MH-8ch | 8 | Pan, PanF, Tilt, TiltF, Dimmer, R, G, B |
| `2` | MH-16ch | 16 | Pan, PanF, Tilt, TiltF, Dimmer, R, G, B, White, Strobe, Gobo, Prism, Focus, Speed, spare ×2 |
| `3` | Par-4ch | 4 | Dimmer, R, G, B |

Channel maps live in `firmware/src/artnet/FixtureProfile.h`; add your own
fixture's DMX mapping there (order + names) without touching the node code.

### Address rules

- `dmxAddress` is 1-based; the node subtracts 1 for the 0-based buffer.
- A fixture's channels must fit within the 512-channel universe or it is
  skipped (`dmxAddress + channels × count ≤ 512`).

## Audio-reactive mapping

When `audioReactive: true`, the current `AudioFrame` drives the universe:

| Channel | Source |
|---|---|
| Dimmer | amplitude × `colorSensitivity` |
| Red / Green / Blue | hue = f(bass, beat) → HSV→RGB, brightness = amplitude |
| White | treble × 0.6 |
| Pan | amplitude swing around center, beat flips direction |
| Tilt | bass swing around center |
| Strobe | beat ? 200 : 0 |
| Gobo | amplitude-scanned positions |
| Prism | beat ? 180 : 0 |
| Focus | fixed mid |
| Speed | `panSpeed` scaled |

Setting `audioReactive: false` keeps the node wired to a console's ArtDmx
(received packets drive the output 1:1), useful for hand-timed shows.

## Status & diagnosis

Boot log prints node status (`artnet` lines); the 3-second `[diag]` line in the
serial console continues to report audio stats. Typical boot lines:

```
[artnet] wifi OK ip=192.168.1.42
[artnet] node started (universe=0, fixtures=2)
```

If the node fails to connect it logs `wifi FAILED` and retries every second —
the LED effects keep running regardless.

## Notes & limits

- Art-Net is **not** DMX512: fixtures must be on an Art-Net-capable path.
- Wi-Fi latency is fine for reactive shows; if you need frame-perfect the DJ
  rig, a wired Art-Net node in front of the fixture is standard practice.
- The ESP32-S3's Wi-Fi is shared with any future web dashboard (Phase 4) — the
  Art-Net task is priority 15, below audio (24) and LED (20).
- RAM cost is small: two 512-byte DMX buffers + the UDP socket + task stack
  (8 KB).