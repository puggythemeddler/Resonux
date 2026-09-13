#!/usr/bin/env python3
"""Resonux Cinematic companion (reference implementation, experimental).

Watches the *program audio* (and optionally a screen region) on the host and
emits SceneFrames over UDP multicast. The ESP32 receives them on
239.255.42.11:9772 and fuses them with its own on-device audio analysis
(Cinematic Mode).

Design rules
------------
* Every heavy dependency is imported lazily so the tool always runs:
    - ``audio_analyzer`` (sounddevice + numpy) -> real host-audio capture.
      Without it the tool falls back to a procedural simulation (``--sim``).
    - ``video_capture`` (mss + numpy) -> screen region for the video channel.
      Without it the video channel stays at "undefined".
* The companion never *analyzes music* like the ESP32 does — it describes the
  *content*: scene kind, discrete events, average luminance, dominant hue,
  motion, and the program-audio level observed on the host. Blending decisions
  are made on the device.
* Audio is the only always-on channel. Video is advisory: a black/frozen screen
  is *not* a signal to kill the lights (the device keeps its own audio fallback).

Wire layout matches sceneframe::Frame in firmware/src/cinema/SceneFrame.h
(packed, little-endian, 30 bytes).

Usage
-----
    python resonux_companion.py --sim --rate 10          # zero-dep smoke feed
    python resonux_companion.py --scene action --event boom --conf 90  # fixed test signal
    python resonux_companion.py --audio --video all --rate 15          # live ingest
"""

import argparse
import math
import random
import socket
import struct
import sys
import time

# ---------------------------------------------------------------------------
# SceneFrame codec (mirrors SceneFrame.h). Pure stdlib on purpose.
# ---------------------------------------------------------------------------

SCENE = {
    "undefined": 0, "speech": 1, "quiet": 2, "action": 3,
    "chase": 4, "explosion": 5, "music": 6,
}
EVENT = {
    "none": 0, "whisper": 1, "flash": 2, "boom": 3, "dark": 4, "change": 5,
}
K_SCENE_MAGIC = 0x53434E52  # 'RNCS' little-endian
K_VERSION = 1
FRAME_FMT = "<IHHII" + "B" * 10 + "2x"  # see SceneFrame.h
FRAME_BYTES = struct.calcsize(FRAME_FMT)
SFLAG_PROGRAM_AUDIO = 0b01
SFLAG_PILLARBOX = 0b10

SCENE_REV = {v: k for k, v in SCENE.items()}
EVENT_REV = {v: k for k, v in EVENT.items()}


def pack_frame(seq, host_ms, scene, event, conf, lum, hue, sat, val,
               motion, prog_audio, source_flags):
    """Returns a 30-byte SceneFrame as bytes."""
    return struct.pack(
        FRAME_FMT, K_SCENE_MAGIC, K_VERSION, 0, seq, host_ms,
        int(scene), int(event), max(0, min(100, int(conf))),
        max(0, min(255, int(lum))), max(0, min(255, int(hue))),
        max(0, min(255, int(sat))), max(0, min(255, int(val))),
        max(0, min(255, int(motion))), max(0, min(255, int(prog_audio))),
        int(source_flags),
    )


# ---------------------------------------------------------------------------
# Procedural sim (zero-dep): a fake "plot" so the pipeline can be smoke-tested
# without a screen or a microphone.
# ---------------------------------------------------------------------------

class SimProgram:
    """Synthesized program audio + screen foreground over a ~plot timeline."""

    def __init__(self):
        self._t = 0.0
        self._level = 0.25
        self._boom_at = -1.0

    def tick(self, dt_ms):
        self._t += dt_ms / 1000.0
        # slow envelope with a bit of noise; rare booms punctuate it
        target = 0.18 + 0.30 * (0.5 + 0.5 * math.sin(self._t * 0.4)) \
            + 0.08 * math.sin(self._t * 2.1)
        self._level += (target - self._level) * 0.15
        if random.random() < .01 and self._t - self._boom_at > 6.0:
            self._boom_at = self._t
            self._level = 0.95
        boom = 0.0 < (self._t - self._boom_at) < 0.35
        lum = int(40 + 150 * (0.5 + 0.5 * math.sin(self._t * 0.5)))
        motion = int(min(255, 12 + 80 * self._level))
        hue = int((self._t * 6.0) % 255)
        scene = SCENE["music"] if self._level > 0.5 else \
            SCENE["quiet"] if self._level < 0.12 else SCENE["speech"]
        event = EVENT["boom"] if boom else EVENT["none"]
        return {
            "level": self._level,
            "lum": lum, "hue": hue, "sat": 200, "val": 200, "motion": motion,
            "scene": scene, "event": event, "conf": 82,
        }


# ---------------------------------------------------------------------------
# Optional host-audio capture (sounddevice + numpy). Lazy import.
# ---------------------------------------------------------------------------

def make_audio_capture(rate=16000, block_ms=32):
    try:
        import audio_analyzer  # lazy: sounddevice + numpy pulled in there
        return audio_analyzer.HostAudio(sample_rate=rate, block_ms=block_ms)
    except (ImportError, OSError) as e:
        print(f"[companion] host audio capture unavailable ({e}) - use --sim",
              file=sys.stderr)
        return None


# ---------------------------------------------------------------------------
# Optional screen capture (mss + numpy). Lazy import.
# ---------------------------------------------------------------------------

def make_video_capture(region=None, scale=32):
    try:
        import video_capture  # lazy: mss + numpy pulled in there
        return video_capture.HostVideo(region=region, scale=scale)
    except (ImportError, OSError) as e:
        print(f"[companion] screen capture unavailable ({e}) - video off",
              file=sys.stderr)
        return None


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

def build_parser():
    ap = argparse.ArgumentParser(description="Resonux Cinematic companion")
    ap.add_argument("--group", default="239.255.42.11", help="multicast group")
    ap.add_argument("--port", type=int, default=9772, help="UDP port")
    ap.add_argument("--rate", type=float, default=10, help="frames per second")
    ap.add_argument("--sim", action="store_true",
                    help="synthesize scene/video instead of capturing")
    ap.add_argument("--audio", action="store_true",
                    help="capture host program audio (sounddevice)")
    ap.add_argument("--video", default="", metavar="X,Y,W,H|all",
                    help="screen region to watch")
    ap.add_argument("--scene", default="", choices=list(SCENE),
                    help="pin a scene kind (for tests); empty = auto")
    ap.add_argument("--event", default="", choices=list(EVENT),
                    help="pin an event kind (for tests); empty = auto")
    ap.add_argument("--conf", type=int, default=80,
                    help="confidence when pinning scene/event")
    ap.add_argument("--once", action="store_true",
                    help="emit N frames and exit (smoke test)")
    ap.add_argument("--frames", type=int, default=10, help="frames for --once")
    ap.add_argument("--verbose", action="store_true")
    return ap


def main(argv=None):
    args = build_parser().parse_args(argv)

    sim = SimProgram() if args.sim else None
    host_audio = make_audio_capture() if args.audio else None
    host_video = make_video_capture(args.video) if args.video else None

    source_flags = SFLAG_PROGRAM_AUDIO if (host_audio or args.sim) else 0

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 4)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)

    seq = 0
    period = 1.0 / args.rate
    next_t = time.monotonic()

    print(f"[companion] {args.group}:{args.port} @ {args.rate} fps "
          f"(audio={'host' if host_audio else 'sim' if args.sim else 'off'}, "
          f"video={'host' if host_video else 'sim' if args.sim else 'off'})")

    try:
        while True:
            now = time.monotonic()
            if now < next_t:
                time.sleep(0.002)
                continue
            next_t += period
            seq += 1

            if args.sim:
                f = sim.tick(period * 1000)
                if host_audio:  # --sim + --audio: keep host audio as the level
                    lvl, _feat = host_audio.tick(period * 1000)
                    f["level"] = lvl
            elif host_audio:
                lvl, feat = host_audio.tick(period * 1000)
                f = {
                    "level": lvl, "scene": SCENE["undefined"],
                    "event": EVENT["none"], "conf": 80,
                    "lum": 0, "hue": 0, "sat": 0, "val": 0, "motion": 0,
                }
                if host_video:
                    vid = host_video.tick(period * 1000, lvl)
                    f.update(vid)
            else:
                f = {
                    "level": 0, "scene": SCENE["undefined"],
                    "event": EVENT["none"], "conf": 80,
                    "lum": 0, "hue": 0, "sat": 0, "val": 0, "motion": 0,
                }

            scene = SCENE[args.scene] if args.scene else f["scene"]
            event = EVENT[args.event] if args.event else f["event"]
            conf = args.conf if args.scene else f.get("conf", 80)

            frame = pack_frame(
                seq, int(now * 1000), scene, event, conf,
                f.get("lum", 0), f.get("hue", 0), f.get("sat", 0),
                f.get("val", 0), f.get("motion", 0),
                int(255 * min(1.0, f.get("level", 0.0))), source_flags,
            )
            sock.sendto(frame, (args.group, args.port))

            if args.verbose:
                print(
                    f"#{seq:06d} {SCENE_REV.get(scene, '?'):<10} "
                    f"{EVENT_REV.get(event, '?'):<7} "
                    f"lum={f.get('lum', 0):3d} mot={f.get('motion', 0):3d} "
                    f"audio={int(255 * min(1, f.get('level', 0))):3d}",
                    file=sys.stderr,
                )
            if args.once and seq >= args.frames:
                print(f"[companion] sent {seq} frames and exiting")
                break
    except KeyboardInterrupt:
        print("\n[companion] stopped")
    finally:
        sock.close()


if __name__ == "__main__":
    main()