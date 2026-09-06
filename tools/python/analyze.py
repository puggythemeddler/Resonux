#!/usr/bin/env python3
"""Audio-analysis lab for the LED controller (Phase 1 mirror).

Replicates the firmware band/beat pipeline in NumPy so bands, beats, and
LED frames can be watched/simulated before touching hardware.

Usage:
  pip install numpy scipy matplotlib
  python analyze.py song.mp3 [--bands 9] [--start 20] [--end 16000]
"""
import argparse
import os
import sys

import numpy as np

try:
    import matplotlib.pyplot as plt
    from scipy.signal import stft
    from scipy.io import wavfile
except ImportError as e:  # pragma: no cover
    sys.exit(f"missing dependency: {e}\nRun: pip install numpy scipy matplotlib")

BAND_LABELS = ("bass", "low-mid", "mid", "high-mid", "treble")


def load_audio(path):
    """Return float32 mono time-domain samples at a fixed target rate."""
    target = 16000
    if path.lower().endswith(".wav"):
        sr, data = wavfile.read(path)
        if data.ndim > 1:
            data = data.mean(axis=1)
        return data.astype(np.float32) / max(1, np.abs(data).max()), sr, target
    # MP3/other: shell out to ffmpeg if present.
    try:
        import subprocess
        import tempfile

        tmp = os.path.join(tempfile.gettempdir(), "led_ana.wav")
        subprocess.run(
            ["ffmpeg", "-y", "-i", path, "-ac", "1", "-ar", str(target),
             "-sample_fmt", "flt", tmp], check=True, capture_output=True)
        sr, data = wavfile.read(tmp)
        return data.astype(np.float32) / max(1, np.abs(data).max()), sr, target
    except Exception as e:
        sys.exit(f"cannot decode {path}: {e}. Convert to WAV/MP3 and install ffmpeg.")


def build_bands(start, end, count):
    edges = np.geomspace(max(start, 20.0), min(end, 20000.0), count + 1)
    return list(zip(edges[:-1], edges[1:]))


def band_energy(sr, data, edges):
    win = int(sr * 0.025)  # 25 ms Hann window
    f, t, Z = stft(data, fs=sr, window="hann", nperseg=win, noverlap=win // 2)
    mag = np.abs(Z)
    out = np.zeros((len(edges) - 1, mag.shape[1]), dtype=np.float64)
    for i in range(len(edges) - 1):
        sel = (f >= edges[i]) & (f < edges[i + 1])
        if sel.any():
            out[i] = mag[sel].max(axis=0)  # peak-hold per band (like firmware)
    return t, out


def normalize_global(x, q=0.999):
    p = np.percentile(x, q * 100)
    return x / p if p > 0 else x


def beat_energy(t, energies, sensitivity=1.3, cooldown=3):
    e = energies.sum(axis=0)
    mean, var = e.mean(), e.var()
    thr = mean + sensitivity * np.sqrt(var) if var > 0 else mean * 1.5
    beats = []
    last = -cooldown
    for i, v in enumerate(e):
        if v > thr and i - last >= cooldown:
            beats.append(i)
            last = i
    return np.array(beats, dtype=int), thr


def led_simulate(t, energies, beats, leds=30):
    """Mini 'Spectrum Analyzer' style frame sim: columns scaled by band energy."""
    n, nb = energies.shape
    frames = np.zeros((n, nb, leds), dtype=np.float32)
    for k in range(nb):
        col = leds // (nb + 1)
        base = k * col
        for i in range(n):
            fill = max(0, min(col - 1, int(round(energies[i, k] * (col - 1)))))
            frames[i, k, base : base + fill] = 1.0
    return frames


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("file")
    ap.add_argument("--bands", type=int, default=5)
    ap.add_argument("--start", type=int, default=20)
    ap.add_argument("--end", type=int, default=16000)
    ap.add_argument("--sensitivity", type=float, default=1.3)
    ap.add_argument("--leds", type=int, default=30)
    args = ap.parse_args()

    samples, sr, _ = load_audio(args.file)
    t, energies = band_energy(sr, samples, build_bands(args.start, args.end, args.bands))
    energies = normalize_global(energies)
    beats, thr = beat_energy(t, energies, args.sensitivity)
    frames = led_simulate(t, energies, beats, args.leds)

    summary = {
        "duration_s": round(t[-1] if len(t) else 0, 2),
        "bands": len(energies),
        "frames": len(t),
        "beat_frames": int(len(beats)),
        "beat_rate_est": round(float(len(beats)) / max(1e-6, t[-1]) * 60, 1),
    }
    print("summary (matches firmware pipeline):")
    for k, v in summary.items():
        print(f"  {k}: {v}")

    fig, ax = plt.subplots(4, 1, figsize=(12, 10))
    ax[0].plot(np.linspace(0, t[-1], len(samples)), samples, lw=0.4)
    ax[0].set_title("waveform")
    img = ax[1].imshow(energies, aspect="auto", origin="lower", cmap="magma",
                       extent=[t[0], t[-1], 0, len(energies)])
    ax[1].set_title("band energy (normalized)")
    fig.colorbar(img, ax=ax[1])
    mid = energies.mean(axis=0)
    ax[2].plot(t, energies.sum(axis=0), lw=0.7)
    ax[2].axhline(thr, color="r", ls="--", lw=1, label="beat threshold")
    for b in beats:
        ax[2].axvline(t[b], color="g", lw=0.5, alpha=0.7)
    ax[2].legend(loc="upper right")
    ax[2].set_title("total energy + detected beats (green ticks)")
    ax[3].imshow(frames.max(axis=1).T, aspect="auto", origin="lower",
                 cmap="Greens", extent=[t[0], t[-1], 0, args.leds])
    ax[3].set_title(f"LED sim ({args.leds} LEDs, max over bands)")
    ax[3].set_xlabel("seconds")
    fig.tight_layout()
    out = os.path.splitext(os.path.basename(args.file))[0] + "_led.png"
    plt.savefig(out, dpi=120)
    print(f"saved plots -> {out}")


if __name__ == "__main__":
    main()