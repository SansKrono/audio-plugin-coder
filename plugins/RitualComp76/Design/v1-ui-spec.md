# UI Specification v1: RitualComp76

## Design Source
- **Library design:** Amber Signal (`design_library/amber-signal`)
- **Style:** Modern minimal flat. No skeuomorphism, single amber accent.
- **Framework:** WebView (HTML5 canvas controls)

## Layout
- **Window:** 760 × 380 px (fixed)
- **Sections (top → bottom):**
  1. **Header**, 48 px: wordmark `RITUALCOMP76` + subtitle `FET LIMITING AMPLIFIER` on the left, **Power** toggle on the right.
  2. **Body**, ~292 px: horizontal strip, left → right, in hardware order:
     `INPUT` | `OUTPUT` | `VU METER + METER MODE` | `ATTACK / RELEASE` | `RATIO`
  3. **Footer**, 28 px (thin): version left, gain-reduction readout right (`GR -4.2 dB`).
- **Grid:** 5 columns, 20 px outer padding, 12 px gaps, thin 1 px dividers between column groups.

```
┌──────────────────────────────────────────────────────────────────────────┐
│ RITUALCOMP76  FET LIMITING AMPLIFIER                             [ ⏻ ]   │
├──────────────────────────────────────────────────────────────────────────┤
│  ╭────╮    ╭────╮  │  ╭───────────────────╮  │  ╭──╮   │  [  4  ]      │
│  │ IN │    │OUT │  │  │    ╲  VU  ╱       │  │  │AT│   │  [  8  ]      │
│  ╰────╯    ╰────╯  │  ╰───────────────────╯  │  ╰──╯   │  [ 12  ]      │
│  INPUT     OUTPUT  │  [GR][+8][+4][OFF]      │  ╭──╮   │  [ 20  ]      │
│  -24 dB    -24 dB  │                         │  │RL│   │  [ ALL ]      │
│                    │                         │  ╰──╯   │               │
├──────────────────────────────────────────────────────────────────────────┤
│ v0.0.1                                                     GR  -4.2 dB   │
└──────────────────────────────────────────────────────────────────────────┘
```

## Controls

| Parameter | Element ID | Type | Position | Size | Range / Values | Default |
|-----------|-----------|------|----------|------|----------------|---------|
| `input` | `input-knob` | Rotary (canvas) | Col 1 | 112 px | ∞, -48 – 0 dB | -24 dB |
| `output` | `output-knob` | Rotary (canvas) | Col 2 | 112 px | ∞, -48 – 0 dB | -24 dB |
| (meter) | `vu-meter` | Canvas needle VU | Col 3 top | 236 × 140 px | -20 – +3 VU | n/a |
| `meter_mode` | `meter_mode-control` | 4-segment toggle | Col 3 below meter | 236 × 28 px | GR / +8 / +4 / OFF | GR |
| `attack` | `attack-knob` | Rotary (canvas) | Col 4 top | 68 px | 1 – 7 | 3 |
| `release` | `release-knob` | Rotary (canvas) | Col 4 bottom | 68 px | 1 – 7 | 5 |
| `ratio` | `ratio-control` | 4 toggles + ALL | Col 5 | 84 × 34 px each | Out / 4 / 8 / 12 / 20 / All | 4 |
| `bypass` | `bypass-control` | Power toggle | Header right | 32 px | On / Off | Off (plugin active) |

### Interaction
- **Knobs:** vertical drag (Shift = fine), mouse wheel, double-click resets to default. 270° sweep. Value readout below each knob.
  - Input/Output show `∞` at the bottom of travel, otherwise `-24 dB`.
  - Attack/Release show the step value plus the real time (`3.0 · 290 µs`, `5.0 · 210 ms`).
- **Ratio:**
  - Click a ratio button to select it (radio behavior).
  - Click the active button again to deselect, which gives **Out** (all buttons out, color only). Every button turns dim and `OUT` shows.
  - Shift-click to add buttons. Two or more pressed → **All**. The `ALL` button is a shortcut. In All mode every button lights in the secondary (cyan) color to warn of smash mode.
- **Meter mode:** segmented control, one active at a time. `OFF` parks the needle at its rest point and dims the meter.
- **Power:** amber when the plugin is active. Bypassed: dim, and the meter needle drops.

## Color Palette
- Background: `#101014`
- Surface: `#1A1A1F`
- Surface highlight: `#292930`
- Primary / accent: `#FFC300`
- Secondary (All mode only): `#00FFF0`
- Text primary: `#F0F0F5`
- Text secondary: `#A0A0A5`
- Border: `#3A3A45`
- Danger (meter > 0 VU zone): `#FF4D3D`

## Style Notes
- Hardware signal-flow order is kept so 1176 users find controls instantly.
- Input/Output are dominant because they are the main "compression amount" controls, since there is no threshold.
- Attack/Release are smaller, matching their secondary role on the original.
- The cyan secondary is reserved for All mode. It is the one strong visual alarm in an otherwise single-accent UI.
- No images and no external fonts: system monospace stack, everything drawn on canvas or in CSS.
