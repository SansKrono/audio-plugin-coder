# Style Guide v1: RitualComp76 (based on Amber Signal)

## Color Palette

| Token | Hex | Use |
|-------|-----|-----|
| `--bg` | `#101014` | Window background, knob wells |
| `--surface` | `#1A1A1F` | Main panel |
| `--surface-hl` | `#292930` | Buttons idle, knob track, meter face |
| `--primary` | `#FFC300` | Active state, knob value arc, needle, power on |
| `--primary-dim` | `rgba(255,195,0,0.16)` | Active button fill, glow |
| `--secondary` | `#00FFF0` | All-buttons mode only |
| `--danger` | `#FF4D3D` | VU scale above 0 |
| `--text` | `#F0F0F5` | Labels, values |
| `--text-dim` | `#A0A0A5` | Captions, scale numbers, units |
| `--border` | `#3A3A45` | 1 px dividers, outlines |

## Typography
- **Family:** `"Roboto Mono", "SF Mono", Menlo, Consolas, monospace`. Roboto Mono is used if installed; nothing is loaded externally.
- Wordmark: 15 px / 700 / letter-spacing 0.18em / uppercase
- Subtitle: 10 px / 400 / 0.14em / `--text-dim`
- Control label: 10 px / 600 / 0.16em / uppercase / `--text-dim`
- Value readout: 13 px / 500 / tabular / `--text`
- Button text: 12 px / 600 / 0.08em
- Meter scale: 9 px / 400 (canvas)

## Spacing
- Outer padding 20 px; column gap 12 px; label → knob 8 px; knob → value 6 px.
- Border radius: 4 px (buttons, panels); knobs circular.
- Dividers: 1 px `--border`, inset 16 px top/bottom.

## Control Styles

### Rotary Knob (canvas)
- Body: flat circle, `--surface-hl` fill, 1 px `--border` ring.
- Track: 270° arc (from -135° to +135°), 3 px, `--bg`.
- Value arc: same radius, 3 px, `--primary`, drawn from min to the current value.
- Pointer: short 2 px line from 35% to 70% of the radius, `--text`.
- Hover: track brightens to `--border`. Drag: value arc gets a soft glow (`shadowBlur 8`, primary).

### Toggle Button (ratio, meter mode)
- Idle: `--surface-hl` fill, `--text-dim` text, 1 px `--border`.
- Hover: border `--text-dim`.
- Active: `--primary-dim` fill, `--primary` border and text.
- All mode: all ratio buttons use the secondary color (`rgba(0,255,240,0.14)` fill, `#00FFF0` border and text).
- Out state: all idle, small `OUT` caption under the stack in `--text-dim`.

### VU Meter (canvas)
- Face: `--bg` with a 1 px `--border` rounded rect.
- Scale arc: thin 1 px `--text-dim` from -20 to 0, `--danger` from 0 to +3. Major ticks at -20, -10, -7, -5, -3, -1, 0, +1, +2, +3.
- Needle: 1.5 px `--primary`, pivot below the visible face. Ballistics are VU-like (~300 ms).
- Mode label top-left inside the face (`GR`, `+8`, `+4`). OFF: whole face at 40% opacity, needle at rest (-20).

### Power Button
- 28 px circle, power glyph drawn in CSS/SVG inline.
- On (active): `--primary` stroke + glow. Bypassed: `--text-dim` stroke.

## Example States
- **Default:** IN -24, OUT -24, ATK 3, REL 5, ratio 4 lit amber, meter GR, needle near 0 VU dipping on peaks.
- **Heavy drive:** IN -6, needle swings to -10 VU, footer `GR -11.8 dB`.
- **All mode:** all four ratio buttons + ALL glow cyan, needle pinned hard left.
- **Bypassed:** power dim, needle rests, values remain visible.
