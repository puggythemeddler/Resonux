#!/usr/bin/env python3
"""Optional screen-region capture for the Resonux companion.

Imports mss + numpy lazily — this module must NOT be required for the
companion to run (the main script catches ImportError and disables video).
"""


class HostVideo:
    """Watches a screen region and reports luminance / dominant colour / motion.

    All numbers are scaled to the SceneFrame 0..255 range. This channel is
    advisory only — a black or frozen screen never forces the lights off.
    """

    def __init__(self, region=None, scale=32):
        import mss
        import numpy as np

        self._np = np
        self._sct = mss.mss()
        if region and region != "all":
            x, y, w, h = (int(v) for v in region.split(","))
            self._mon = {"left": x, "top": y, "width": w, "height": h}
        else:
            self._mon = self._sct.monitors[0]
        self._scale = scale
        self._prev = None
        self._prev_time = time_ms()

    def _grab(self):
        np = self._np
        shot = np.asarray(self._sct.grab(self._mon))
        # collapse to a small square-ish grid: (h, w) luminance + hue approx
        small = shot[:: max(1, shot.shape[0] // self._scale),
                     :: max(1, shot.shape[1] // self._scale)]
        return small.astype(np.float32)

    def tick(self, dt_ms, level):
        np = self._np
        small = self._grab()
        lum = float(np.mean(np.mean(small, axis=2)))     # 0..255 bg average
        r, g, b = (small[..., i].mean() for i in range(3))
        mx, mn = max(r, g, b), min(r, g, b)
        sat = (mx - mn) / max(1e-6, mx) * 255.0
        if mx == mn:
            hue = 0.0
        elif mx == r:
            hue = 60.0 * (((g - b) / max(1e-6, mx - mn)) % 6) / 360.0 * 255.0
        elif mx == g:
            hue = 60.0 * ((b - r) / max(1e-6, mx - mn) + 2) / 360.0 * 255.0
        else:
            hue = 60.0 * ((r - g) / max(1e-6, mx - mn) + 4) / 360.0 * 255.0
        val = mx
        motion = 0.0
        spatial = None
        if self._prev is not None:
            diff = np.abs(small - self._prev)
            motion = min(255.0, float(np.mean(diff)) * 20.0)
            # pixel-resolved motion map -> on-screen focus + zone salience.
            # The map is already downscaled (screen/scale), so the reductions
            # are cheap. focusX/focusY is the argmax column/row; zones carry
            # the fraction of the motion energy per screen region.
            dmap = diff.mean(axis=2)
            total = float(dmap.sum()) + 1e-6
            h, w = dmap.shape
            rows = dmap.sum(axis=1) if h else np.zeros((1,), float)
            cols = dmap.sum(axis=0) if w else np.zeros((1,), float)
            focus_x = float(np.argmax(cols)) / max(1, w) if w else 0.5
            focus_y = float(np.argmax(rows)) / max(1, h) if h else 0.5
            zones = []
            if w:
                thirds = [float(cols[:w // 3].sum()),
                          float(cols[w // 3:2 * w // 3].sum()),
                          float(cols[2 * w // 3:].sum())]
                best = max(range(3), key=lambda i: thirds[i])
                # 0,1,2 = LEFT,CENTRE,RIGHT (SceneFrame ZoneId)
                zones.append((best, int(100 * thirds[best] / total)))
            if h:
                halves = [float(rows[:h // 2].sum()),
                          float(rows[h // 2:].sum())]
                tb = max(range(2), key=lambda i: halves[i])
                # 3 = TOP, 4 = BOTTOM
                zones.append((3 if tb == 0 else 4,
                              int(100 * halves[tb] / total)))
            if motion > 60.0:
                zones.append((9, int(100.0 * min(1.0, motion / 180.0))))
            spatial = {
                "focusX": int(focus_x * 255),
                "focusY": int(focus_y * 255),
                "zones": zones[:3],
            }
        self._prev = small
        now = time_ms()
        dts = max(1.0, (now - self._prev_time) / 1000.0)
        self._prev_time = now
        # soft scene guess from motion/luminance (advisory)
        if motion < 3.0 and lum < 40.0:
            scene = 2  # quiet
        elif motion > 90.0:
            scene = 4  # chase
        elif motion > 40.0:
            scene = 3  # action
        elif lum > 180.0 and level > 0.5:
            scene = 6  # music
        else:
            scene = 1  # speech
        return {
            "lum": int(lum), "hue": int(hue), "sat": int(sat), "val": int(val),
            "motion": int(motion), "scene": scene, "conf": 75,
            "spatial": spatial,
        }


def time_ms():
    import time

    return time.monotonic() * 1000.0