# Productisation — Carrier PCB + Enclosure + CE (Phase 10)

Status: **design doc in repo**. Roadmap row 10, acceptance: "design-docs +
BOM for production; EMC/UL notes". Decisions locked with the owner:

- **Board strategy**: carrier/HAT PCB — the commodity
  ESP32-S3-DevKitC-1 (N16R8) plugs in; the carrier carries mic,
  touchscreen, LED/PSU outputs, protection and power distribution.
- **Enclosure**: commodity ABS/DIN box with a cutout set.
- **Compliance posture**: CE self-certification + RoHS/REACH technical file.
  The ESP32-S3-WROOM(S)-1 module is already RF-certified by Espressif, so RF
  work is mostly antenna placement review.

Prototype-first: validate the carrier as a 2-layer dev splice before any
production tooling. Every electrical figure below traces back to
`docs/HARDWARE.md` §1–§6 and `docs/BOM.md`.

## 1. System concept

```
 ┌──────────────┬──────────────────────────────────────────────┐
 │  Carrier PCB  │                                              │
 │  (no MCU)     │  INMP441 (I2S)   │  USB-C (flash/console)   │
 │               │  Touch 3.5" TFT  │  ESP32-S3-DevKitC-1 (HAT)│
 │  ┌─────────┐  └──────────────────┴───────────────▲─────────┐│
 │  │5 V rail │  Addressable DIN (WS281x/APA102)     │ J1      ││
 │  │ fuses   │  Conventional RGB (PWM/MOSFET)  ─────┤ stack   ││
 │  │ TVS/ESD │  Single-colour (PWM)                ──┤──╌╌╌╌───││
 │  │ reverse │  Art-Net (WiFi)                      │         ││
 │  └─────────┘  5 V PSU input (barrel/screw)  ◄──────┘         ││
 └──────────────┴───────────────────────────────────────────────┘
```

Only two boards to the end user; one PSU. The carrier re-uses the default
pin plan of `docs/HARDWARE.md` §7 plus the `display` block pins
(`docs/TOUCHSCREEN.md`).

## 2. Carrier PCB (prototype splice → production)

Recommended start: **2-layer, 65×55 mm, ≥ 74 mil / >200 µm** FR-4, 2&nbsp;oz
copper on the LED-power net, mounting holes M3 in corners
(DevKitC birdsnest clearances).

### Connectors (all on one pinout block, labelled on silkscreen)

| Function | Connector | Pins | Wires to |
|---|---|---|---|
| PSU in | DC barrel 2.1/5.5 **or** 5 mm screw terminal | 2 (5 V, GND) | 5 V PSU (fused) |
| USB-C passthrough | USB break-out pads → DevKitC USB, cutout | | flashing/console |
| Mic (INMP441 module) | JST-PH 2.0 | 4 (BCLK/WS/SD/VDD), GND via rail | GPIO 4/5/6 |
| Addressable LED | JST-XH 3-pin | DATA, 5 V, GND | WS281x / SK6812 / APA102 |
| Conventional RGB | 4-pin screw/JST | R, G, B PWM, GND | MOSFET bank (below) |
| Single-colour | JST-XH 2-pin | PWM, GND | mono strip |
| Touchscreen 3.5" | 2×8 male header / FPC | SPI + I2C + BL + 3.3 V | panel (`display` pins) |

Power feed of each output is **individually fused** at the connector
(see §3). LED data/clock lines get a series resistor at the connector
(330 &Omega; WS281x; APA102 SCK 22 &Omega; + MOSI 33 &Omega;).

### On the Mic

Keep the 4 I2S lines off the LED return path; 100 nF per INMP441 on the
carrier near the connector. VDD from the 3.3 V rail (LDO on carrier).

### On the touchscreen

The reference panel needs 5 V backlight + 3.3 V logic. Backlight is a
separately-fused 5 V tap (screen-only, per `DisplayConfig`); the LEDC
signal (`blPin=21`) goes straight to the module's BL pin. SPI ≤ 40 MHz —
keep the run < 5 cm, series 22 &Omega; on SCK/MOSI, GND guard between SPI
and I2C.

### Conventional RGB (Phase 6) MOSFET bank on the carrier

Low-side N-MOSFET per colour per zone (IRLZ34N), gate resistors 100–220 &Omega;,
10 k&Omega; gate pull-downs (off at boot), flyback not required for resistive
loads but add a 100 nF across each zone. See HARDWARE.md §5.

## 3. Power distribution & protection

Single **5 V** bulk rail on the carrier:

1. **Reverse-polarity + inrush**: series Schottky (or PMOS ideal-diode) at the
   barrel; NTC inrush limiter (e.g. 1 &Omega;@1 A) for the bulk caps.
2. **Bulk + HF**: 1000 µF 25 V electrolytic + 0.1 µF ceramic per output
   connector (mirror of HARDWARE.md §3.1).
3. **Fusing**: blade fuse or soldered SMD fuse per output (addressable 5 A,
   RGB 3 A/zone, screen 1 A, board 1 A) + one 5 A master upstream.
4. **ESD/TVS**: bidirectional TVS on each data/clock line to GND at the
   connector; a common-mode choke optional on the mic I2S.
5. **3.3 V**: low-drop LDO from 5 V rail (AMS1117-3.3 class, ≥ 500 mA),
   decoupled at the DevKitC power headers; never feed the strip from it.
6. **Grounding**: single star ground near the PSU input; LED return wires come
   back to the same point, NOT through the DevKitC. Length of LED return kept
   ≤ PSU input length (voltage-drop rules of HARDWARE.md §4 still apply).

Everything above is a **proposal** for the schematic splice — it is exactly
the circuit of HARDWARE.md §6 shifted onto a board.

## 4. Enclosure (commodity, off-the-shelf)

Reference: ABS project box ≈ 130 × 80 × 38 mm with a vented lid (or DIN-rail
variant for use in racks).

- **Cutouts**: USB-C (dev), DC barrel, LED panel/gland (PG7) for strip leads,
  optional SMA for external antenna.
- **Antenna clearance**: keep ≥ 15 mm of air and no continuous metal over the
  DevKitC ceramic antenna zone (right edge of the board); route antenna edge
  toward a wall of the box, not the PSU.
- **Thermal**: Power LEDs are off-board; the box only dissipates the carrier's
  modest heat — two small vent slots behind the LDO/PSU entry are enough.
- **Mounting**: M3 standoffs in the PCB holes; PSU either inside (wing mount)
  or external via barrel.
- **Label**: rear label with device name, "Class B, CE", input rating,
  RoHS mark, and the web/AP quick-start (`Resonux` hotspot).

## 5. User manual outline (to author at production build)

1. Safety & electrical limits (HARDWARE.md §1/§6 condensed — *no warranty*).
2. Hookup: mic → strips → PSU → power on.
3. First power: AP hotspot `Resonux`, browse `http://192.168.4.1/`.
4. Themes, per-strip effects, brightness (web + touchscreen).
5. Multi-controller sync (master/slave, group/port) — `docs/MULTI_CONTROLLER.md`.
6. Art-Net fixtures.
7. Firmware updates (HTTP OTA + ArduinoOTA).
8. Troubleshooting + diagnostics (`[diag]` line).

## 6. Manufacturing notes

- **Who**: proto run at JLCPCB/PCBWay-class fab; assembly by hand for the
  first 10, then optional SMALL line when volumes justify.
- **Package**: `.step` + gerbers, split BOM by (fab) / (assembly) / (owner —
  DevKitC + panel + PSU), centroid + pick&place for the MOSFET/LDO/TVS parts.
- **Test jig**: DevKitC already gives us the firmware; hand-check is:
  apply 5 V, check rails, flash, run the 3–5 minute soak per HARDWARE.md §8.
- **BOM continuity**: price-drift guard — keep 2nd sources for LDO/fuses/TVS.

## 7. CE self-certification + RoHS (self-assessment posture)

The ESP32-S3-WROOM(S)-1 module carries Espressif's own FCC/CE/RoHS RF
certifications; the carrier adds only a small switch-mode-free power path
(LDO, no PFC). Self-assessment scope:

- **EN 55032** (emissions, Class B): radiated + conducted measurements
  optional for a low-volume studio product; at minimum document design rules
  (single star ground, TVS snoopers, antenna placement) and keep a test
  record if a unit is measured. Schedule one paid EMC session at a local lab
  before high-volume commitment.
- **EN 55035** (immunity): rely on module + TVS + fused inputs; document.
- **Low Voltage / SAFETY**: 5 V SELV — out of LV-directive scope; still keep
  HARDWARE.md §1 fusing.
- **RoHS/REACH**: component-level declarations; RoHS mark on label; keep a
  technical file: schematics, BOM with cas numbers where relevant,
  module cert PDFs, test records.
- **Radio**: module certified — attach the Espressif modular-approval letter
  to the technical file; no separate RF testing needed.

> Compliance language here is **guidance, not legal advice**; confirm with the
> relevant notified body before any commercial sale claim.

## 8. Open decisions (owner)

- Exact enclosure SKU + internal PSU bracket vs external PSU.
- Whether the prototype splice ships as a **depopulated carrier kit** first.
- Fusing format (blade vs SMD fuse holder vs resettable PTC).
- Lab EMC session budget/location before manufacturing scale-up.