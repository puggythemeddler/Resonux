# Resonux brand identity

The Resonux brand is built on one idea:

> **Resonance becomes visible as light.**

The logo is the *Luminous Node at Resonance*: a resonant ring holds a
standing-wave filament that winds to the cavity wall and ignites into a
single point of warm light. It says music, without an equalizer, a note, or
a wave — and it carries the name inside itself: Resonux ↔ *lux* (light).

## Mark

- **Ring** — the chamber where the music resonates.
- **Filament** — the standing wave, drawn as a single flowing curve.
- **Node** — the point where the wave becomes light: the single pixel of
  emotion the product creates.
- **Tile** — dark graphite hardware; the mark is the light (*graphite
  instrument panel, one warm filament accent*).

## Palette

| Role | Value | Hex |
|---|---|---|
| Tile top (gradient) | graphite upper | `#232831` |
| Tile bottom (gradient) | graphite lower | `#14171b` |
| Tile border | rim | `#2e3540` |
| Ring + filament | amber filament | `#f59a3e` |
| Ignition node | warm pale | `#ffe9cf` |
| Monochrome mark (dark surfaces) | off-white | `#eef0f4` |
| Monochrome mark (light surfaces) | near-black | `#1c2026` |

Amber (`#f59a3e`) is the product's only warm accent; the UI accent derives
from `--accent-h: 30` in `desktop/ui/styles.css`.

## Geometry (256-unit design space)

- Tile: `8,8` 240×240, corner radius 56.
- Ring: centre `(128,130)`, radius 86, stroke 10, round caps.
- Filament: `M 63 192 C 100 192 98 136 128 130 S 162 84 192 74`.
- Node: centre `(192,74)`, radius 17.

## Asset set (`desktop/build/`)

| Asset | Use |
|---|---|
| `icon.ico` | Windows exe + installer icon (16/24/32/48/64/128/256, PNG entries) |
| `icon.png` | 1024 px tile master (shared, document) |
| `logo.svg` | primary lockup: tile + mark |
| `logo-mark.svg` | standalone full-colour mark (transparent) |
| `logo-mark-dark.svg` | monochrome mark for dark surfaces |
| `logo-mark-light.svg` | monochrome mark for light surfaces |
| `wordmark.svg` / `wordmark-light.svg` | RESONUX wordmark (graphite / off-white) |
| `logo-lockup.svg` / `logo-lockup-light.svg` | mark + wordmark combinations |
| `logo-mark-128.png` | transparent mark PNG (picklists, small UI) |
| `favicon.png` | transparent mark PNG (web, Electron window) |

## Usage rules

- No equalizer / generic "R" / waveform / RGB-bar / note / headphone / lamp
  clichés — the mark above is the only logo.
- Amber is the only warm accent; keep monochrome variants for print/stamps.
- The renderer's in-app mark (navrail, wizard) is `desktop/ui/icons.tsx`
  `Logo`; it mirrors `logo.svg` exactly — keep the two in sync.

## Source of truth

`desktop/build/make-icon.ps1` regenerates the whole set with no binary inputs:

```
powershell -ExecutionPolicy Bypass -File build\make-icon.ps1
```

Run from `desktop/`. PS 5.1 notes: drawing functions must not take a
`[switch]` parameter (a switch-param function whose body returns .NET draw
objects mis-binds a call-operator result), and constructors are written with
`[Type]::new(...)` casts rather than `New-Object Type(args)`.