# Style Guide v2: RitualAnvil (GlassKit, forge aurora)

## Theme override (loaded after `glasskit.css`, `<html data-theme="dark">`)

GlassKit is re-branded through its own tokens only; no component CSS is rewritten.

| GlassKit token | v2 value | Effect |
|---|---|---|
| `--gl-color-primary` | `#FFC300` (amber) | value arcs, focus ring, primary badges, meters |
| `--gl-color-primary-dark` | `#FF5A1F` (ember) | gradient end, glows: amber→ember everywhere GlassKit mixes |
| `--gl-color-bg-light` / `-mid` / `-dark` | `#2a1a12` / `#1a110d` / `#0c0807` | charcoal-brown base gradient |
| `--gl-bg-aurora-1` | `rgba(255, 90, 31, 0.20)` | ember aurora (left) |
| `--gl-bg-aurora-2` | `rgba(255, 195, 0, 0.10)` | amber aurora (right) |
| `--gl-bg-aurora-3` | `rgba(150, 70, 30, 0.22)` | rust glow (top) |
| `--gl-bg-light-streak-1/2` | `rgba(255, 150, 70, 0.10)` / `0.07` | warm light streak |
| `--gl-bg-warm-glow` | `rgba(255, 90, 31, 0.10 → 0.32)` | **live:** rises with effective Heat + Mix Ready |

## Anvil tokens

| Token | Value | Use |
|---|---|---|
| `--anvil-ember` | `#FF5A1F` | Mix Ready contribution: ghost arcs, effective readouts, effective EQ curve, Heat glow |
| `--anvil-well` | `rgba(0, 0, 0, 0.28)` | recessed wells: knob groove, EQ curve, meter troughs |
| `--anvil-danger` | `#FF4D3D` | meter top 3 dB, ceiling line, limiter GR |
| `--anvil-card-pad` | `12px 14px` | stage card padding (GlassKit default 40/24 px is for app screens) |
| `--anvil-card-radius` | `18px` | stage card corners (GlassKit 24 px) |

Text, muted text, surfaces 1–5, borders, blur and shadows are GlassKit's own (`--gl-color-text`, `--gl-color-text-muted`, `--gl-surface-*`, `--gl-border-*`, `--gl-blur*`, `--gl-shadow-card`).

## Typography
- **Family:** GlassKit `--gl-font-family` (system UI stack).
- Wordmark: 17 px / 700 / 0.16em / uppercase; `ANVIL` in primary
- Subtitle: 11 px / 500 / 0.14em / muted
- Card title: 11 px / 650 / 0.14em / uppercase, after a `.glass-badge--primary` number
- Control label: 10 px / 600 / 0.12em / uppercase / muted
- Value: 12 px / 600 / tabular-nums / text
- Effective readout: 11 px / 600 / tabular-nums / ember
- Hero value: 44 px / 700 / tabular-nums; `%` 15 px muted

## Control styles

### Glass knob (canvas)
- **Groove:** 270° arc, 4 px, `--anvil-well`, with a 1 px `rgba(255,255,255,0.10)` inner edge
- **Value arc:** 4 px, linear gradient amber → ember along the arc (`--gl-color-primary` → `--gl-color-primary-dark`); bipolar from neutral; soft amber glow while dragging
- **Ghost arc:** 4 px `--anvil-ember` on the groove from manual → effective, with an ember dot at the end
- **Disc:** radial gradient `rgba(255,255,255,0.22)` (top-left) → `rgba(255,255,255,0.06)`; 1.5 px rim `--gl-border-medium`; 1 px highlight arc along the top-left edge (`rgba(255,255,255,0.45)`); drop shadow `0 4px 12px rgba(0,0,0,0.35)`
- **Pointer:** 2.5 px white line from 30 % to 68 % of the disc radius, round caps
- **Hover:** rim brightens to `--gl-border-strong`

### Hero dial
- 12 px groove; amber → ember value arc with glow `shadowBlur = 6 + 22·t`
- Big frosted disc (as the knob) with ticks every 10 % around it (passed ticks in text colour)
- Centre: value + `%`; caption `RAW` / `FORGING`

### Heat knob
- Glass knob + radial ember glow behind the disc, alpha = effective drive / 20

### Meters
- IN / OUT: 16 px troughs in `--anvil-well`, rounded; fill amber → ember gradient; top 3 dB `--anvil-danger`; peak hold 1 px white; dBFS scale between
- GR bars: 8 px troughs; ember (Punch) / danger (Ceiling), fill from the top

### EQ curve
- Recessed glass well (`--anvil-well`, 12 px radius, inset shadow); grid in `rgba(255,255,255,0.06)`
- Manual curve amber; effective curve ember with a faint ember fill (only while Mix Ready > 0)

### Power
- `.glass-pill`; on = amber icon + `--gl-shadow-btn-primary`-like glow; bypassed = muted icon, body at 55 % opacity

## Example states
- **Default:** quiet forge, dim warm glow, all knobs neutral, flat amber EQ line.
- **Mix Ready 75 %, Heat 6:** hero arc amber→ember with a strong glow, ember ghost arcs on EQ/Punch/Heat/Width, the aurora's ember glow visibly brighter.
- **Bypassed:** body 55 % opacity, power pill muted.
