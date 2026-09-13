#!/usr/bin/env python3
"""Optional host-audio capture for the Resonux companion.

Imports sounddevice + numpy lazily — this module must NOT be required for the
companion to run (the main script catches ImportError and falls back to --sim).
"""

import math
import time


class HostAudio:
    """Streams host (program) audio and produces levels + feature flags.

    Feature thresholds are deliberately *loose* — this class only reports what
    the program audio is doing: level, boom/impact, whisper. Scene/event
    decisions on the device re-check against on-board audio anyway.
    """

    def __init__(self, sample_rate=16000, block_ms=32):
        import numpy as np
        import sounddevice as sd

        self._np = np
        self._sd = sd
        self._buffer = np.zeros(int(sample_rate * 0.4), dtype=np.float32)
        self._rate = sample_rate
        self._last_impact_ms = -99999.0
        self._slow = 0.0
        self._stream = sd.InputStream(
            samplerate=sample_rate, channels=1, dtype="float32",
            blocksize=sample_rate * block_ms // 1000,
            callback=self._cb,
        )
        self._stream.start()

    def close(self):
        self._stream.stop()
        self._stream.close()

    def _cb(self, indata, frames, time_info, status):
        # keep a short rolling window; indata is (frames, channels)
        n = len(indata[:, 0])
        self._buffer[:-n] = self._buffer[n:]
        self._buffer[-n:] = indata[:, 0]

    def tick(self, dt_ms):
        np = self._np
        now = time.monotonic() * 1000.0
        buf = self._buffer
        rms = float(np.sqrt(np.mean(buf ** 2)))
        slow = rms if rms > self._slow else self._slow * 0.98
        self._slow = slow
        level = max(0.0, min(1.0, rms * 6.0))
        boom = rms > slow * 2.5 + 0.05 and (now - self._last_impact_ms) > 450
        whisper = 0.0 < level < 0.12
        if boom:
            self._last_impact_ms = now
        return level, {
            "boom": boom,
            "whisper": whisper,
            "tension": float(max(0.0, min(1.0, (rms * 12.0) - 0.4))),
        }