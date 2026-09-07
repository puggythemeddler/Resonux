# Hardware Design & Electrical Safety — Music-Reactive LED Controller

> **Read this before wiring anything.**
> The ESP32 produces **control signals**. It never carries LED power.
> Software power-limiting is decoration; fuses, correct PSU sizing, MOSFETs and
> wiring are the real protection. This document gives worked numbers.

---

## 1. System power architecture

```
          AC Mains
             │
             ▼
   ┌─ 5V 25A PSU (LEDs) ──┬─────► strip segment 1 (+5V)   [fused]
   │                       ├─────► strip segment 2 (+5V)   [fused]
   │                       └─────► strip segment 3 (+5V)   [fused]
   │
   └─ 5V-rail → ESP32-S3 dev-board VIN/5V pin   (board handles 3V3 regulation)
                    │
                    └── common ground (star/bus) → strip + mic + MOSFET sources

   CONTROL SIGNALS (never power!):
   ESP32 GPIO ──(in-line resistor)──► LED strip DATA / MOSFET gates
```

**Grounding is the #1 build rule:** every 5V PSU negative, every LED segment
GND pad, the ESP32 GND, the mic GND, and every MOSFET source must be tied to
one common star/bus ground. Without it, data glitches, flicker, and phantom
boots appear everywhere.

---

## 2. Bill-of-materials prototype (Phase 1)

| Qty | Part | Purpose |
|---|---|---|
| 1 | ESP32-S3-DevKitC-1 **N16R8** (16 MB flash, 8 MB PSRAM) | main controller |
| 1 | INMP441 I2S MEMS microphone | ambient audio capture |
| 1 | WS2812B strip, **30 LEDs/m, 1–2 m** (don't buy 20 m yet) | addressable prototype |
| 1 | 5 V 3 A PSU (prototype; short strip ≈ 0.9 A full white/30 LEDs) | LED power |
| . | Breadboard + 0.22 mm² (24 AWG) or larger jumper wires | wiring |
| 1 | USB-C data cable | flashing + serial |
| 1 | 330 Ω resistor (data line) | signal damping on DIN |
| 1 | 1000 µF / 25 V electrolytic + 1× 0.1 µF ceramic | bulk decoupling at strip power input |
| opt. | 74AHCT125 level shifter (3.3→5 V data) | only if data is unreliable on long runs |

Phase-6 add-ons (conventional RGB test, §5): logic-level N-MOSFET kit
(IRLZ44N or TO-220 `IRLZ34N`), gate resistors, 5 V RGB strip 1–2 m, second 5 V
PSU or shared rail with fused branches.

---

## 3. Addressable LEDs (WS2812B / WS2811 / WS2815 / SK6812 / APA102)

### 3.1 WS281x / SK6812 (one-wire)

```
   +5V PSU ──┬──────────────► +5V strip input
             │  [1000µF][0.1µF]
             ▼  (decoupling, as close to strip as possible)
   ESP32 S3  GND ──► strip GND        (common ground)
   ESP32 S3  (GPIO48) ──330Ω──► DIN    (data)
```

- **Bulk cap:** 1000 µF electrolytic (+0.1 µF ceramic) across +5/GND at the
  strip's power input; keep it *near the strip connector*, not the dev-board.
- **Data resistor:** 330 Ω in series with DIN damps ringing on longer wires.
- **Level shifting:** WS2812B needs ≥3.5 V for reliable logic-1. A 1–2 m strip
  off a 3.3 V S3 GPIO usually works at low power; for runs >2 m / suspicion of
  glitches insert a **74AHCT125** buffer (5 V side → strip). Do this *and*
  keep the common ground. APA102/HD107 also accept 3.3 V logic but the same
  guidance applies.
- **Power injection:** >300 LEDs ≈ 18 A at full white ⇒ inject +5 V at both
  ends (and every ~100 LEDs on long runs) with 1.5 / 2.5 mm² leads. Use fuses
  per injection branch.
- **Never power the strip from the dev-board 3V3/5V pins** (they are
  regulators, not supplies).

### 3.2 WS2815 / SK6812 variants

- WS2815 is a 12 V cousin of WS2812 using the **same 800 kHz protocol** — the
  driver runs it on the WS2812 controller (`chipset=WS2815` in config); wiring
  identical in spirit (12 V rail, bulk cap, injection at 12 V with
  corresponding math).
- SK6812 RGBW uses 4 bytes/LED. The current build maps `ORDER_RGBW` to GRB
  (white channel not driven until FastLED exposes native RGBW), so treat
  RGBW strips as RGB for now; wiring same.

### 3.3 APA102 / HD107 (clocked SPI family)

```
   ESP32  (GPIO data) ──► CI/DIN   +5V strip input (fused) ──► +5V
   ESP32  (GPIO clock) ──► CO/CLK  ESP32 GND ──► GND
```

Level shift both data **and** clock to 5 V for long runs. Decoupling and
injection same as above.

---

## 4. Worked power calculations (Phase 1 and a big install)

| Case | Math | Result |
|---|---|---|
| 30 LEDs (1 m, WS2812B) all white | 30 × 60 mA | 1.8 A ⇒ **3 A 5 V** PSU ✓ |
| 60 LEDs (2 m) full white | 60 × 0.06 | 3.6 A ⇒ **5 A** PSU |
| 150 LEDs (5 m) full white | 150 × 0.06 | 9 A ⇒ **10 A / 12 A** PSU, inject mid-strip |
| 300 LEDs (10 m) full white | 300 × 0.06 | 18 A ⇒ **20 A+** PSU, inject every 100 LEDs |
| Practical "music" load | ~30 % of full-white duty | margins covered by above sizing |

**Power = Volts × Amps:** 12 V RGB strip at 3 A per colour-channel pair →
12 × 3 = **36 W** per channel-driven section.

**Wire sizing (copper, simple rule):**

| Conductor | Max safe | Typical duty |
|---|---|---|
| 0.5 mm² (20 AWG) | ~5 A | short jumper / per-injection stub |
| 1.0 mm² (17 AWG) | ~10 A | main run ≤ 2 m |
| 1.5 mm² (15/16 AWG) | ~15 A | mains of a mid install |
| 2.5 mm² (13/14 AWG) | ~20–25 A | high-current injection trunks |

**Voltage drop:** for a run, ΔV = 2 × I × (resistivity per metre × length).
Allow ≤5 % drop (e.g. ≤0.25 V on a 5 V rail). When drop + brownout appears
(dimming tail, flicker), inject power closer to the tail rather than raising
PSU voltage.

**Fusing:** fuse each PSU branch near its source: `A_fuse = 1.25 × branch max
current`. Prototype: inline blade fuse 3–5 A at each strip input.

---

## 5. Conventional RGB strips (Phase 6) — MOSFET low-side switching

Common low-side N-MOSFET stage per colour channel (use **logic-level** FETs):

```
   ESP32 GPIO ──[100–220Ω gate R]──► MOSFET gate
                                     MOSFET drain ──► RGB strip channel (-)
                                     MOSFET source ──► common ground
                                     (10k gate-pulldown to GND recommended to
                                      keep LED off during MCU boot/float)

   +V PSU ──► strip + terminal   (R,G,B channel - leads to each driver)
              strip GND ──► separate return to same star ground
```

- One stage **per zone per colour**: 3-zone RGB strip = 9 MOSFETs + 9 gate Rs.
- MOSFET selection (logic-level, Vgs(th) ≤ 2 V, Rds(on) low):
  - small current: **2N7002** (only ≤300 mA/channel, SMD, heatsink-less);
  - medium (up to ~10 A/channel @ 12 V): **IRLZ34N / IRLZ44N** (TO-220, will
    need modest heatsinking near max);
  - strong: **IRLB3034** (excellent Rds(on)) with heatsink.
- **Common anode vs common cathode:** the *strip* decides. Our firmware
  inverts PWM (`commonAnode`) for anode strips; the MOSFET always switches the
  *return* path (low side), which works for both configurations as long as the
  strip's anodes/cathodes are connected to the rail and the -channels run
  through drains.
- No GPIO powers anything: the PSU rails the strip; GPIOs only toggle gates.
- RGB vs 5 V/12 V: same stage; only rail voltage and wire gauge change. Verify
  the chosen FET's Vgs spec is satisfied at the exact VDD used (3V3 GPIO high
  ≈ 3.3 V ⇒ logic-level FETs only).

### Single-colour strips (Phase 7)

Same low-side stage, one MOSFET per section. Brightness = PWM duty. Beat/band
maps to duty in the single-colour driver. MOSFET + gate R exactly as above.

### High-power / constant-current modules (future Phases)

LED drivers with a **dimmable/PWM input** (e.g. Mean Well HLG-style `DIM`
input, or a small constant-current board such as the ubiquitous AMC7135 / PT4115
class) are driven from the same PWM, or from a `PWM driver`-specific subclass.
The LED abstraction already isolates this; wiring is per-module datasheet.

---

## 6. Protection & app notes

- **Reverse polarity:** a series Schottky (prototype) or reverse-polarity
  P-MOSFET ideal-diode stage; at minimum fuse + clearly marked polarity
  connectors.
- **Decoupling:** 1000 µF electrolytic + 0.1 µF ceramic per powered segment /
  per MOSFET power rail near the gates; a 0.1 µF close to the ESP32 3V3 pin.
- **Level shifting** as in §3.1 when driving 5 V-logic strips or long data.
- **Heat:** TO-220 MOSFETs near their rated current need heatsinks; verify by
  touch after 30 min full-white.
- **Connectors:** screw/WAGO for power; do not rely on breadboard for >2 A.
- **Brownouts during full white:** the classic symptom of an undersized PSU —
  re-check §4, don't just raise brightness in software.
- **Input protection:** optional TVS on the PSU rails; note in final product
  design (Phase 10).

---

## 7. Phase-1 pin plan (ESP32-S3-DevKitC-1)

| Function | Pin | Notes |
|---|---|---|
| I2S mic BCLK  | GPIO4 | |
| I2S mic WS/LRCLK | GPIO5 | |
| I2S mic DATA | GPIO6 | |
| WS2812B data | GPIO48 | RMT output; avoids S3 strapping pins (0/3/45/46) & USB (19/20) |
| (Phase 6) Zone1 R/G/B PWM | GPIO10/11/12 | example; any non-reserved pin |
| Serial (USB-C) | on-board | 115200 baud |

S3 strapping/USB pins to **avoid** for LED/audio: 0, 3, 19, 20, 45, 46.

Addressable pins are compiled into the firmware: data `1,2,3,5-21,33-42,47,48`
(default GPIO48), APA102 clock `4,8,10,13,15,18,33,38,47,48`. A pin outside
these sets makes `begin()` fail cleanly to avoid driving an uninstantiated
output.

---

## 8. Checklist before applying power

- [ ] All grounds tied to common star point (ESP32, PSU, strip, mic, FETs).
- [ ] Fuse(s) in place on each LED supply branch.
- [ ] Bulk capacitor mounted at strip power input.
- [ ] Data line resistor (addressable) / gate resistors (analog) fitted.
- [ ] ESP32 powered from its own regulated rail, strip from the PSU rail.
- [ ] No GPIO connected to a rail without a current-limiter in the path.
- [ ] Software power limits configured; hardware is the last line anyway.