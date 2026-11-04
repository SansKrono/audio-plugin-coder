# UI Specification v1 — VaporStrings

> The source of truth is `v1-ui-map.json`. This file is the human-readable summary. Edit the map and re-compose rather than editing this file.

## Layout
- **Window:** 1040 × 640 px at 1× (plate 65 × 40 cells, 16 px per cell)
- **Mode:** `scalable`, aspect-locked
  - Scale range 0.5×–2.0×. Presets: 50 / 75 / 100 / 125 / 150 / 200 %.
  - In-page corner grip. Letterbox fill: `--surface`.
- **Grid:** two bands under a header, laid out left-to-right in signal order.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ VAPORSTRINGS        [ ~~~ horizon sun-grid + output waveform ~~~ ]  ◀ PRESET ▶ │  header  y0–6.25
├────────────┬─────────────────────────────┬──────────────────────────────────┤
│ INPUT 入力  │ SOURCES 音源                  │ VOICE 声                          │  band 1  y6.5–24
│ in   gate  │ DIRECT SQUARE | OCT OCT-1 +1 SH│ CUT RES DRIVE ENVAMT [LP24]       │
│ hpf  sens  │ SYNTH MIX DET PW UNI SPREAD    │ [FOLLOW] [ env-curve canvas ]     │
│ spd  glide │ [OSC1][OCT][OSC2][OCT]  SUB    │ ATK DEC SUS REL PICKVEL           │
│ [PRIORITY] │                               │                                   │
├────────────┴────┬─────────────────────────┴──────────────┬───────────────────┤
│ VAPOR 蒸気        │ FX エフェクト                              │ OUTPUT 出力        │  band 2  y24.5–39.5
│ [tracking disp] │ SLOW DRIVE ENSEMBLE WOBBLE HAZE ECHO MOD │ WIDTH  DRY/WET    │
│      (VAPOR)    │ [on][knobs of selected page] [fx-scope]  │     OUTPUT        │
│   hero knob     │                                          │ [IN/OUT meters]   │
└─────────────────┴──────────────────────────────────────────┴───────────────────┘
```

### Sections (cells)
| ID | Title | Cell (x, y, w, h) |
|---|---|---|
| `header` | — | 0, 0, 65, 6.25 |
| `input` | INPUT 入力 | 0.5, 6.5, 11.5, 17.5 |
| `sources` | SOURCES 音源 | 12.5, 6.5, 27, 17.5 |
| `voice` | VOICE 声 | 40, 6.5, 24.5, 17.5 |
| `core` | VAPOR 蒸気 | 0.5, 24.5, 17, 15 |
| `fx` | FX エフェクト | 18, 24.5, 36.5, 15 |
| `output` | OUTPUT 出力 | 55, 24.5, 9.5, 15 |

### FX tab system
- **Tabs:** seven chips at y26: `tab-slow`, `tab-drive`, `tab-ens`, `tab-wob`, `tab-haze`, `tab-echo`, `tab-mod`. Each switches the shared page area (x18.5–42, y29–39).
- **Hidden pages:** every page's controls are real DOM elements with a `view` tag. Only the active view is displayed (toggled via `data-view` on the FX section).
- **Default view:** `fx-haze`.
- **Status LEDs:** each tab chip carries an LED that reflects its `*_on` param. MOD lights when either LFO amount ≠ 0.
- **`fx-scope` canvas** (x42.5, y29.25, 11.5×9.5) is always visible and draws the active effect.

## Controls

### Header
| Control | Type | Cell | Notes |
|---|---|---|---|
| `title` | label | 1, 1, 14, 2.5 | Display font, chrome gradient |
| `subtitle` | label | 1, 3.75, 14, 1.5 | Katakana + "GUITAR → SYNTH" |
| `horizon` | canvas | 16, 0.5, 33, 5.5 | Output waveform on the sun-grid |
| `preset-prev` / `preset-next` | chip | 50 / 62, 1.75, 2.5, 2.25 | Steps through factory presets |
| `preset-name` | screen | 52.75, 1.75, 9, 2.25 | Click opens the preset list |

### Input
| Parameter | Type | Cell | Range | Default |
|---|---|---|---|---|
| `in_gain` | knob | 1, 8 | −24…+24 dB | 0 |
| `gate_thresh` | knob | 6.5, 8 | −90…0 dB | −60 |
| `track_hpf` | knob | 1, 12.5 | 40–400 Hz | 70 |
| `track_sens` | knob | 6.5, 12.5 | 0–1 | 0.5 |
| `track_speed` | knob | 1, 17 | 0–1 | 0.6 |
| `glide` | knob | 6.5, 17 | 0–2000 ms | 30 |
| `note_priority` | flip | 1, 21.75, 10.5×2 | Last/Lowest/Highest | Last |

### Sources
| Parameter | Type | Cell | Range | Default |
|---|---|---|---|---|
| `src_direct_lvl` | knob | 13, 8 | −inf…+6 dB | −6 |
| `direct_square` | knob | 17.25, 8 | 0–1 | 0 |
| `src_oct_lvl` | knob | 21.5, 8 | −inf…+6 dB | −inf |
| `oct_down` | knob | 25.75, 8 | 0–1 | 0.5 |
| `oct_up` | knob | 30, 8 | 0–1 | 0 |
| `oct_shift` | knob | 34.25, 8 | ±24 st | 0 |
| `src_synth_lvl` | knob | 13, 13 | −inf…+6 dB | 0 |
| `osc_mix` | knob | 17.25, 13 | 0–1 | 0.5 |
| `osc2_detune` | knob | 21.5, 13 | ±100 ct | 7 |
| `pulse_width` | knob | 25.75, 13 | 0.05–0.95 | 0.5 |
| `unison` | knob (stepped) | 30, 13 | 1–7 | 1 |
| `unison_spread` | knob | 34.25, 13 | 0–1 | 0.3 |
| `osc1_wave` | flip | 13, 19.5 | Saw…Sine | Saw |
| `osc1_oct` | flip | 18.75, 19.5 | −2…+2 | 0 |
| `osc2_wave` | flip | 23, 19.5 | Saw…Sine | Square |
| `osc2_oct` | flip | 28.75, 19.5 | −2…+2 | 0 |
| `sub_lvl` | knob | 33.25, 18.75 | 0–1 | 0 |

### Voice
| Parameter | Type | Cell | Range | Default |
|---|---|---|---|---|
| `flt_cutoff` | knob | 40.5, 8 | 20 Hz–20 kHz (log) | 4000 |
| `flt_res` | knob | 45, 8 | 0–1 | 0.2 |
| `flt_drive` | knob | 49.5, 8 | 0–1 | 0 |
| `flt_env_amt` | knob (bipolar) | 54, 8 | −1…+1 | 0.3 |
| `flt_type` | flip | 59, 8.5 | LP24/LP12/BP/HP | LP24 |
| `env_mode` | flip | 40.5, 13.5 | Follow/ADSR/Swell | Follow |
| `env-curve` | canvas | 46.5, 13, 17.75×4.5 | — | — |
| `env_attack` / `env_decay` / `env_sustain` / `env_release` | knob | 40.5 / 45 / 49.5 / 54, 18.5 | see spec | — |
| `vel_sens` | knob | 58.5, 18.5 | 0–1 | 0.5 |

### VAPOR core
| Element | Type | Cell | Notes |
|---|---|---|---|
| `tracking` | canvas | 1, 26, 16×4 | Note, cents, confidence, chord warning |
| `vapor` | knob (hero) | 4.5, 30.25, 9×9 | Macro, 0–1, default 0 |

### FX pages (shared area x18.5–42, y29–39)
| View | Controls |
|---|---|
| `fx-slow` | `slow_on` psw · `slow_pitch`, `slow_drift` knobs |
| `fx-drive` | `drive_on` psw · `drive_mode` flip · `drive_amt`, `drive_tone` |
| `fx-ens` | `ens_on` psw · `ens_mode` flip · `ens_rate`, `ens_depth`, `ens_mix` |
| `fx-wob` | `wob_on` psw · `wob_wow`, `wob_flutter`, `wob_age`, `wob_dropout` |
| `fx-haze` *(default)* | `haze_on`, `haze_freeze` psw · `haze_size`, `haze_decay`, `haze_predelay`, `haze_mix` / `haze_mod`, `haze_damp`, `haze_reverse` |
| `fx-echo` | `echo_on`, `echo_sync` psw · `echo_div` flip · `echo_time`, `echo_fb`, `echo_tone`, `echo_mix` |
| `fx-mod` | LFO 1 row: `lfo1_shape`, `lfo1_dest` flips · `lfo1_rate`, `lfo1_amt` · `lfo1_sync` psw. LFO 2 row: same set. |

When `echo_sync` is on, `echo_time` dims and `echo_div` is active. When it is off, the reverse applies. LFO rate behaves the same way with `lfoN_sync`.

### Output
| Parameter | Type | Cell | Range | Default |
|---|---|---|---|---|
| `width` | knob | 55.25, 26 | 0–2× | 1 |
| `dry_wet` | knob | 59.75, 26 | 0–1 | 1 |
| `out_gain` | knob | 57.5, 30.75 | −24…+12 dB | 0 |
| `io-meters` | canvas | 55.5, 35.5, 8.5×3.5 | IN / OUT bars | — |

**Coverage:** all 82 parameters in `parameter-spec.md` have a control. The meters (`m_note`, `m_conf`, `m_env`, `m_in`, `m_out`) feed canvases only.

## Color Palette
- Background (letterbox/surface): `#0d0221`
- Plate: `#1a0b2e`
- Inset / wells: `#12052b`
- Rule lines: `#5b2a86`
- Primary accent (hot pink): `#ff71ce`
- Secondary accent (cyan): `#01cdfe`
- Tertiary (mint): `#05ffa1`
- Violet: `#b967ff`
- Sun yellow: `#fffb96`
- Warn: `#ff4f6d`
- Text: `#f6e9ff` (dim: `#a58bc7`)

## Style Notes
- **Vaporwave neon:** deep purple night, a pink → cyan neon, a striped retro sun, and a perspective grid.
- **Bilingual section titles** (English + Japanese) as part of the aesthetic.
- **Effects are restricted to CSS** (gradients, box-shadow glow, a scanline overlay via `repeating-linear-gradient`), plus canvas for the visualisers. No images or external fonts are required, though a display font may be bundled at impl.
- **Glow is reserved for active state.** Idle controls are dim violet; touched, automated or "on" controls glow pink or cyan.
- **Layout constraints** (from the layout system): rem cells only, no px in authored markup, and all JS inline in one `<script>`.
