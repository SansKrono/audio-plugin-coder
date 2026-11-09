# UI Specification v2: RitualAnvil (GlassKit)

## Design Source
- **Library:** GlassKit 1.40.0 (`_tools/GlassKit`, MIT, CSS-only glassmorphism). Used unmodified: `glasskit.css` + an Anvil theme override loaded after it.
- **Base:** v1 layout, controls, IDs and behaviour (ghost arcs, effective readouts, EQ curve, meters) carried over unchanged.
- **Decisions (user, v2 brief):** rotary knobs kept and restyled as glass discs · forge-tinted aurora background · dark only · 1100 × 660.
- **Preview:** `Design/v2-test.html` (links `_tools/GlassKit/glasskit.css`; serve from the repo root).

## What GlassKit provides vs what Anvil draws

| Element | Source |
| :--- | :--- |
| Page backdrop (aurora + light streak + warm glow) | `.glass-bg`, re-tinted via `--gl-bg-*` tokens |
| Stage modules, meters column | `.glass-card` (padding/radius tightened for plugin density) |
| Mix Ready hero | `.glass-card.glass-card--glow` |
| Stage numbers `01`–`07`, MACRO tag, footer readouts | `.glass-badge`, `.glass-badge--primary` |
| Bypass / power | `.glass-pill` (46 px) with an "on" state |
| Section dividers | `.glass-divider` |
| Rotary knobs, hero dial, EQ curve, meters | Anvil canvas, drawn to match GlassKit surfaces/borders/tokens (GlassKit has no knob) |

## Layout
- **Window:** 1100 × 660 px, fixed
- **Header (54 px):** wordmark `RITUAL ANVIL` + subtitle `DRUM BUS FORGE` left; power `.glass-pill` right. No bar background: it floats on the aurora.
- **Body:** three columns, 14 px gaps, 22 px side padding, 12 px top / 14 px bottom
  - **Left (300 px):** hero card *Mix Ready* (glow card, flex) above card *01 Input* (172 px)
  - **Center (flex, ≈ 650 px):** two rows of stage cards in signal order
    - Row 1: *02 Transient* (196 px) · *03 Tone EQ* (rest, EQ curve on a recessed glass well)
    - Row 2: *04 Punch* · *05 Heat* · *06 Width* · *07 Ceiling* (equal width)
  - **Right (84 px):** IN / OUT meters card
- **Footer (32 px):** version badge + latency badge left, limiter GR badge right

```
┌──────────────────────────────── forge aurora ───────────────────────────────┐
│ RITUAL ANVIL  DRUM BUS FORGE                                           (⏻)  │
│ ╭─────────────╮ ╭──────────╮ ╭──────────────────────────────────╮ ╭──────╮  │
│ │ MIX READY   │ │[02] TRANS│ │[03] TONE EQ  ~~~~ curve ~~~~     │ │IN OUT│  │
│ │   ( ◉ )     │ │ ◎    ◎   │ │ ◎    ◎    ◎    ◎    ◎            │ │▮   ▮ │  │
│ │    0 %      │ ╰──────────╯ ╰──────────────────────────────────╯ │▮   ▮ │  │
│ ╰─────────────╯ ╭────────╮ ╭────────╮ ╭────────╮ ╭──────────╮    │▮   ▮ │  │
│ ╭─────────────╮ │[04]PUNCH│ │[05]HEAT│ │[06]WIDTH│ │[07]CEILING│   │▮   ▮ │  │
│ │[01] INPUT   │ │ ◎  ▮GR  │ │  ◎🔥   │ │   ◎    │ │ ◎   ▮GR   │   │      │  │
│ │ ◎ TRIM ◎ HPF│ ╰────────╯ ╰────────╯ ╰────────╯ ╰──────────╯    ╰──────╯  │
│ ╰─────────────╯                                                            │
│ (v0.3.0) (LAT 157 smp)                                   (LIM GR  0.0 dB)  │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Controls (unchanged from v1 except size)
| Parameter | Type | Position | Size | Range | Default |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `mix_ready` | Hero glass dial | Left, hero card | 220 px | 0–100 % | 0 |
| `input_gain` | Glass knob, bipolar | 01 Input | 66 px | ±12 dB | 0 |
| `hpf` | Glass knob, log | 01 Input | 66 px | 20–100 Hz | 40 |
| `transient_attack` | Glass knob, bipolar | 02 Transient | 74 px | ±12 dB | 0 |
| `transient_sustain` | Glass knob, bipolar | 02 Transient | 74 px | −12…+6 dB | 0 |
| `eq_kick` … `eq_air` | Glass knob, bipolar | 03 Tone EQ | 60 px | ±12 dB | 0 |
| `punch` | Glass knob + comp GR bar | 04 Punch | 80 px | 0–100 % | 0 |
| `heat` | Glass knob, ember glow with drive | 05 Heat | 80 px | 0–20 dB | 0 |
| `width` | Glass knob, bipolar (0 at 100 %) | 06 Width | 80 px | 50–150 % | 100 |
| `ceiling` | Glass knob + limiter GR bar | 07 Ceiling | 80 px | −1.0…−0.1 dBTP | −0.3 |
| `bypass` | `.glass-pill` power | Header right | 46 px | on/off | off |

Display-only elements, interaction and element IDs are identical to v1 (`[param]-control`, `-knob`, `-value`, `-eff`), so `Source/ui/public/js/index.js` binds to v2 with renderer changes only.

## New in v2
- **Living forge:** the aurora's warm glow (`--gl-bg-warm-glow`) and the ember aurora layer brighten with effective Heat and Mix Ready. Updated only when the value moves by ≥ 2 % (backdrop blur re-renders when the page background changes).
- **Glass knobs:** frosted disc with a top-left specular highlight and rim, a recessed groove track, amber→ember gradient value arc, ember ghost arc for the macro.
- **Badges** replace v1's plain stage numbers and footer text.

## Risks
- **Blur cost in the plugin's WebView:** about 10 `backdrop-filter` layers. Static background → composited once; the living-forge glow re-blurs only on ≥ 2 % change. Check CPU/GPU in a DAW during Phase 4 conversion; fallback is GlassKit's `data-surface="solid"` (tokens only, no blur).
- **Fonts:** GlassKit's system stack (SF Pro on macOS, Segoe UI on Windows). Values use `font-variant-numeric: tabular-nums`.
