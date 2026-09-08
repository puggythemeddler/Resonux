# Bill of Materials

Prices are representative (2026, prototyping). **Short-strip prototype first** —
do not buy a 20 m reel for development; expand after Phase 1 works.

## Phase 1 — audio + one addressable strip

| Qty | Item | Example | Notes | ~Cost |
|---|---|---|---|---|
| 1 | ESP32-S3 dev board | ESP32-S3-DevKitC-1 **N16R8** | 16 MB flash + 8 MB PSRAM mandatory for later web/multi-strip | $12–18 |
| 1 | I2S MEMS mic | **INMP441** module (e.g. Gravity/DFRobot) | 24-bit, I2S, 3.3 V | $2–4 |
| 1–2 m | WS2812B strip | 30 LEDs/m, 5 V | keep short for dev | $3–6/m |
| 1 | 5 V PSU | 5 V 3 A (short strip) / 5 V 5 A | sized from §2 table; never dev-board rail | $8–14 |
| 1 | 330 Ω resistor | | DIN damping | $0.05 |
| 1 | 1000 µF 25 V + 0.1 µF | electrolytic + ceramic | strip decoupling | $1 |
| . | Breadboard + wires | 24 AWG solid | | $6 |
| 1 | USB-C data cable | | flashing/console | $3 |
| opt | 74AHCT125 level shifter | | long data runs | $1–2 |

**Subtotal ≈ $40–55**

## Phase 6 add-on — conventional RGB test (second hardware test, spec §37)

| Qty | Item | Example | Notes |
|---|---|---|---|
| 1–2 m | 5 V RGB LED strip (common cathode) | smd 5050, 60 LEDs/m | 3 channels |
| 3–9 | Logic-level N-MOSFET | IRLZ34N/IRLZ44N | per colour per zone |
| 3–9 | 100–220 Ω gate resistors | | |
| 3–9 | 10 kΩ gate pull-downs | | off at boot |
| 1 | DC barrel→screw terminal pigtail | | clean power connection |
| opt | 5 V 10 A PSU | | when zones are at full white |
| opt | Small heatsinks | | TO-220 near max |

The same audio/effect engine drives this — proving universality.

## Phase 7 add-on — single-colour strip

1–2 m 5 V (or 12 V) single-colour strip + 1 logic-level MOSFET + gate parts.

## Common consumables

Jumper wires, solder, fuse holders + blade fuses (inline 3 A/5 A), wire spool
(0.5 / 1.0 mm²), WAGO/screw terminals, heat-shrink.

## Tools

- USB-C cable, multimeter (current clamp optional), bench PSU optional,
  soldering iron (later phases).
- Free: PlatformIO + VSCode, git.

## Phase 10 add-on - carrier PCB + enclosure + CE kit (production splice)

Prices representative; fab quotes vary. See `docs/PRODUCTISATION.md` for the
full design doc.

| Qty | Item | Example | Notes |
|---|---|---|---|
| 5 pcs | Carrier PCB | 65x55 mm 2-layer FR4, 2 oz | JLCPCB-class; any colour |  (~$2 each) |
| 5 pcs | ESP32-S3-DevKitC-1 N16R8 | module owner-fitted | MCU side of the stack |
| 1 | 5 V PSU | 5 V 5 A (per rail budget) | from A§2 sizing |
| 1 | ABS/DIN enclosure | 130x80x38 mm class | vented; antenna clearance |
| 1 | USB-C passthrough cable set | | flashing/console cutout |
| 1 | DC barrel + screw terminal pigtails | | PSU entry |
| 1 | Fuse set | master 5 A + per-output SMD/PTC | protection per A§3 |
| 1 | TVS/ESD + reverse-pol parts | bidirectional TVS, Schottky | per connector |
| 1 | LDO + decoupling set | AMS1117-3.3 class | 3.3 V rail |
| opt | IRLZ34N + gate parts | | conventional RGB zones |
| opt | 3.5 inch ILI9488 + FT6236 panel | | touchscreen build |
| 1 | RoHS/REACH technical file pack | | CE self-cert records |