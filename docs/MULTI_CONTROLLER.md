# Multi-Controller Sync — UDP Multicast (Phase 9)

Status: **code in repo, builds green** for both envs (default + touchscreen).
Hardware bench pending. One controller runs the microphone (or network audio) as
**master**; every other controller renders exactly the same `AudioFrame` as
**slave**, so multiple light rigs stay beat-locked without cables.

## How it works

```
MASTER (mic)                            SLAVE (no mic, network audio)
 audio task -> AudioAnalyzer             sync task -> joins multicast group
   -> publishFrame()                          -> SyncClock offset estimate
        |                                        -> injectRemoteFrame()
        v                                           |
   broadcast packed AudioFrame ~31 Hz               v
   via UDP multicast  ------------------->   led task renders (same theme/effects)
```

- The wire format is a compact fixed-size packed struct
  (`sync/SyncProtocol.h`, ~176 B) carrying bands, peaks, group levels,
  beat, beatStrength, a master **frame counter** (`seq`) and the master
  **`timeMs`** timestamp.
- The slave's `SyncClock` estimates the fixed offset
  `local - master` from packet latency (assumed-symmetric LAN transit) and
  rewrites `timeMs` into the local timeline (`toLocal`) — so beat-locked
  effects decay identically to the master's. First packet seeds the estimate;
  later samples converge exponentially. `uint32` wraparound is handled, so
  controllers that boot at different times still lock.
- The slave skips mic/FFT init entirely (saves RAM/CPU) and renders purely
  from the network frame. If the master goes quiet past `timeoutMs`, the last
  frame holds (no fake motion); diagnostics surface `sync=slave alive=0`.
- `publishFrame()`/injection reuse the existing mutex double-buffer in `App`,
  so the real-time LED loop is untouched (still non-blocking `takeFrame`).

## Configuration (`config.json` → `sync`)

```json
"sync": {
  "enabled": true,
  "role": 1,
  "group": "239.255.42.9",
  "port": 9769,
  "heartbeatMs": 32,
  "timeoutMs": 600
}
```

`role`: `0` off, `1` **master** (broadcast), `2` **slave** (listen).

> Role is fixed at boot from config.json. To re-badge a unit, change `role`
> and reboot (dashboard Configuration tab → Save & reboot).

- All members must be on the same LAN (Wi-Fi isolation off).
- Master and slave must agree on `group` + `port`. `heartbeatMs` only matters
  on the master; `timeoutMs` only on slaves.
- The web dashboard shows live sync status on `/api/status` → `sync`
  (`role`, `seq`, `offsetMs`, `masterAlive`); the serial `[diag]` line does
  too every 3 s.

## Wiring / hardware

None — it is pure software on the existing boards.

## Behavioural guarantees

- No changes to the real-time LED path: sync hands frames into the same
  double-buffer the mic used, so themes, per-strip effects, brightness and
  the touchscreen all behave identically on master and slave.
- Only one master per group is supported (no leader election).
- A slave without WiFi still boots (sync disabled, local mic used).
- Art-Net audio-reactivity runs on the **master's** mic pipeline; a slave does
  not re-broadcast DMX (send Art-Net from the master if needed).