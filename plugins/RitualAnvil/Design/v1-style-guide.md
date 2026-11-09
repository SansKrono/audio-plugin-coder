# Style Guide v1: RitualAnvil (based on Amber Signal)

## Color Palette

| Token | Hex | Use |
|-------|-----|-----|
| `--bg` | `#101014` | Window background, knob tracks, meter wells |
| `--surface` | `#1A1A1F` | Main panel |
| `--card` | `#151519` | Stage cards |
| `--surface-hl` | `#292930` | Knob bodies, button idle |
| `--primary` | `#FFC300` | Manual value arcs, meters, EQ manual curve |
| `--primary-dim` | `rgba(255,195,0,0.16)` | Active fills, glows |
| `--ember` | `#FF5A1F` | Mix Ready contribution (ghost arcs, effective readouts, effective EQ curve), Heat glow, hero dial gradient end |
| `--ember-dim` | `rgba(255,90,31,0.18)` | Ember glows |
| `--danger` | `#FF4D3D` | Meter top 3 dB, ceiling line |
| `--text` | `#F0F0F5` | Labels, values |
| `--text-dim` | `#A0A0A5` | Captions, units, scale |
| `--border` | `#3A3A45` | 1 px card outlines, dividers |

## Typography
- **Family:** `"Roboto Mono", "SF Mono", Menlo, Consolas, monospace` (nothing loaded externally)
- Wordmark: 15 px / 700 / 0.18em / uppercase; `ANVIL` in `--primary`
- Subtitle: 10 px / 400 / 0.14em / `--text-dim`
- Card title: 10 px / 600 / 0.16em; stage number in `--primary` at 70 % opacity
- Control label: 9 px / 600 / 0.14em / uppercase / `--text-dim`
- Value: 11 px / 500 / tabular / `--text`
- Effective readout: 10 px / 500 / tabular / `--ember`
- Hero value: 40 px / 700 / tabular; `%` 14 px `--text-dim`

## Spacing
- Outer padding 16 px vertical, 20 px horizontal; column/card gap 12–16 px
- Card: 1 px `--border`, radius 4 px, padding 10 px 12 px, title row 18 px
- Label → knob 6 px; knob → value 4 px

## Control Styles

### Rotary Knob (canvas)
- Body: flat circle `--surface-hl`, 1 px `--border` ring
- Track: 270° arc, 3 px, `--bg` (hover: `--border`)
- Value arc: 3 px `--primary`; unipolar from min, **bipolar from the neutral value** (0 dB, 100 % width)
- **Ghost arc:** 3 px `--ember` on the same track from manual → effective position, with a 2.5 px ember dot at the effective end; drawn over the value arc
- Pointer: 2 px `--text`, 35 % → 72 % radius (shows manual position)
- Drag: value arc glow (`shadowBlur 8`)

### Hero Dial — Mix Ready (canvas, 210 px)
- 10 px arc track `--bg`; value arc linear gradient `--primary` → `--ember` along sweep
- 11 tick marks (every 10 %), dim; ticks under the value arc turn `--text`
- Center: value 40 px + `%`; caption `MIX READY`
- Glow: `shadowBlur = 4 + 20·t`, ember

### Heat Knob
- Standard knob plus radial ember glow behind the body, alpha = effective drive / 20

### Meters (canvas)
- IN / OUT: 16 px wide wells `--bg`, fill `--primary`, top 3 dB `--danger`; peak-hold tick 1 px `--text`
- Scale −48, −24, −12, −6, −3, 0 between bars, 9 px dim
- OUT: 1 px `--danger` line at ceiling
- GR bars (Punch, Ceiling): 8 px wide, fill from top downward, `--ember` (Punch) / `--danger` (Ceiling), 0 / −6 / −12 labels

### EQ Curve (canvas)
- Log freq 20 Hz–20 kHz, ±15 dB, 0 dB line `--border`, grid at 100 / 1k / 10k
- Manual curve 1.5 px `--primary`; effective curve 1.5 px `--ember` when Mix Ready > 0, with faint ember fill to 0 dB

### Power Button
- Same as RitualComp76: 30 px circle, amber glow when on (processing), dim when bypassed

## Example States
- **Default:** Mix Ready 0 %, all knobs neutral, no ember anywhere, flat EQ line, meters moving.
- **Mix Ready 75 %:** hero arc amber→ember, Kick shows ember arc to +3.0, Box ember back to −2.3, Punch ghost to 58 %, Heat ghost to 2.3 dB with faint glow, Width ghost to 110 %, ember EQ curve with low shelf-ish bump + air lift.
- **Bypassed:** body 55 % opacity, power dim.
