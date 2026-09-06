#!/usr/bin/env python3
"""PSU / wire / fuse sizing for the LED controller (docs/HARDWARE.md §4 mirror).

usage: python power_calculator.py [--leds 60] [--type rgb] [--volt 5 --amp]...
"""
import argparse

LED_CURRENT_MA = {"rgb": 60.0, "mono": 20.0, "white": 20.0}  # per-LED at full white, worst case
MANUFACTURER_TOL_PCT = 1.25  # datasheet average vs real-world overdrive
WIRE_AMPACITY_A = [
    (18, 2.5, 5.0),   # AWG 18 chassis-free run
    (20, 1.9, 3.7),
    (22, 1.4, 2.9),
    (24, 1.0, 2.1),
]


def wire_size(amps, run_m, drop_frac=0.08):
    """Select AWG meeting ampacity and < drop_frac voltage drop."""
    for awg, chassis, races in WIRE_AMPACITY_A:
        if amps > chassis:
            continue
        drop = (amps * 2 * run_m) / (58.0 * awg_mm2(awg))  # ~ copper 58 MS/m
        if drop <= drop_frac:
            return awg, round(drop, 3)
    return None, None


def awg_mm2(awg):
    return 0.20268 * 92.0 ** ((36 - awg) / 19.0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--leds", type=int, default=60)
    ap.add_argument("--type", choices=LED_CURRENT_MA, default="rgb")
    ap.add_argument("--volt", type=float, default=5.0)
    ap.add_argument("--wire-meters", type=float, default=2.0)
    ap.add_argument("--max-duty", type=float, default=1.0,
                    help="peak effect duty cycle (1.0 = full white worst case)")
    a = ap.parse_args()

    if a.volt == 0:
        a.volt = 5.0
    na_current = a.leds * LED_CURRENT_MA[a.type] * 1e-3 * MANUFACTURER_TOL_PCT
    peak_a = na_current * a.max_duty
    power_w = peak_a * a.volt
    fuse_a = peak_a * 1.25
    awg, drop = wire_size(peak_a, a.wire_meters)

    print(f"LEDs: {a.leds} x {a.type} @ {a.volt} V")
    print(f"nominal full-white current: {na_current:.2f} A")
    print(f"peak design current (duty {a.max_duty:.0%}): {peak_a:.2f} A")
    print(f"PSU rating (min, +20% margin): {peak_a * 1.2:.2f} A / {power_w * 1.2:.1f} W")
    print(f"fuse: {fuse_a:.2f} A (slow-blow) at strip feed")
    if awg:
        print(f"wire: AWG {awg} for {a.wire_meters} m run, {drop:.1%} drop")
    else:
        print("wire: no single run meets budget — shorten run, split feed, or go thicker")

    print("\nSafety reminders:")
    print(" - 5 V runs use a SEPARATE PSU/ground from the ESP32 data paths.")
    print(" - Fuse at the PSU terminals first, then near the strip for long runs.")
    print(" - Do NOT inject power from the dev board's LDO.")


if __name__ == "__main__":
    main()