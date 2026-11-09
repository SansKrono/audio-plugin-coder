# UI Specification v1: RitualAnvil

## Design Source
- **Library design:** Amber Signal (`design_library/amber-signal`), same base as RitualComp76
- **Addition:** ember accent (`#FF5A1F`) for everything Mix Ready contributes
- **Preview:** `Design/v1-test.html`

## Layout
- **Window:** 1000 × 600 px, fixed
- **Header (48 px):** wordmark `RITUAL ANVIL`, subtitle `DRUM BUS FORGE`, power (bypass) button right
- **Body (524 px):** three columns, 16 px gap, 20 px side padding
  - **Left (268 px):** hero card *Mix Ready* (flex) above card *01 Input* (fixed)
  - **Center (flex, ~584 px):** two rows of stage cards in signal order
    - Row 1: *02 Transient* (180 px) · *03 Tone EQ* (rest, with EQ curve display)
    - Row 2: *04 Punch* · *05 Heat* · *06 Width* · *07 Ceiling* (equal width)
  - **Right (76 px):** IN / OUT vertical peak meters, dBFS scale, ceiling marker on OUT
- **Footer (28 px):** version + latency left, limiter GR readout right

```
┌──────────────────────────────────────────────────────────────────────────┐
│ RITUAL ANVIL  DRUM BUS FORGE                                         (⏻) │
├───────────────┬─────────────┬────────────────────────────────────┬───────┤
│   MIX READY   │ 02 TRANSIENT│ 03 TONE EQ  ~~~~~curve~~~~~        │ IN OUT│
│     ( ◯ )     │  ATK   SUS  │ KICK  BOX  SNARE  PRES  AIR        │ ▮   ▮ │
│      0 %      ├──────────┬──┴───────┬──────────┬───────────────┤ ▮   ▮ │
├───────────────┤ 04 PUNCH │ 05 HEAT  │ 06 WIDTH │ 07 CEILING    │ ▮   ▮ │
│ 01 INPUT      │  ◯  ▮GR  │   ◯🔥    │    ◯     │  ◯   ▮GR      │ ▮   ▮ │
│  TRIM   HPF   │          │          │          │               │       │
├───────────────┴──────────┴──────────┴──────────┴───────────────┴───────┤
│ RITUALANVIL v0.0.0 · LAT 0 smp                         LIM GR  0.0 dB │
└──────────────────────────────────────────────────────────────────────────┘
```

## Controls
| Parameter | Type | Position | Size | Range | Default |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `mix_ready` | Hero dial | Left, hero card | 210 px | 0–100 % | 0 |
| `input_gain` | Knob, bipolar | 01 Input | 64 px | ±12 dB | 0 |
| `hpf` | Knob, log | 01 Input | 64 px | 20–100 Hz | 40 |
| `transient_attack` | Knob, bipolar | 02 Transient | 72 px | ±12 dB | 0 |
| `transient_sustain` | Knob, bipolar (0 at 2/3) | 02 Transient | 72 px | −12…+6 dB | 0 |
| `eq_kick` | Knob, bipolar | 03 Tone EQ | 60 px | ±12 dB | 0 |
| `eq_box` | Knob, bipolar | 03 Tone EQ | 60 px | ±12 dB | 0 |
| `eq_snare` | Knob, bipolar | 03 Tone EQ | 60 px | ±12 dB | 0 |
| `eq_presence` | Knob, bipolar | 03 Tone EQ | 60 px | ±12 dB | 0 |
| `eq_air` | Knob, bipolar | 03 Tone EQ | 60 px | ±12 dB | 0 |
| `punch` | Knob + comp GR bar | 04 Punch | 80 px | 0–100 % | 0 |
| `heat` | Knob, glows with drive | 05 Heat | 80 px | 0–20 dB | 0 |
| `width` | Knob, bipolar (0 at 100 %) | 06 Width | 80 px | 50–150 % | 100 |
| `ceiling` | Knob + limiter GR bar | 07 Ceiling | 80 px | −1.0…−0.1 dBTP | −0.3 |
| `bypass` | Power button | Header right | 30 px | on/off | off |

### Display-only elements
| Element | Source | Notes |
| :--- | :--- | :--- |
| EQ curve | HPF + 5 bands | Amber = manual, ember = effective (with Mix Ready) |
| Macro ghost arcs | effective values | Ember segment on each knob track from manual → effective position |
| Effective readout | effective values | Second line under value, ember, e.g. `→ +6.0`; hidden when equal |
| Punch GR bar | parallel comp GR | 0 to −20 dB, vertical |
| Limiter GR bar | limiter GR | 0 to −12 dB, vertical; also footer readout |
| IN / OUT meters | peak / true-peak | −48…0 dBFS; OUT shows ceiling line |

## Interaction
- Drag vertically on knob (Shift = fine), mouse wheel, double-click = default.
- Power button toggles bypass; whole body dims to 55 % when bypassed.
- Element IDs follow `[param]-control`, `[param]-knob`, `[param]-value` (+ `[param]-eff`).

## Color Palette
- Background `#101014` · Surface `#1A1A1F` · Card `#151519`
- Primary (amber) `#FFC300` · Ember (macro / heat) `#FF5A1F`
- Text `#F0F0F5` · Text dim `#A0A0A5` · Border `#3A3A45` · Danger `#FF4D3D`

## Style Notes
- Flat, no skeuomorphism; forge feel comes from the amber → ember gradient on the hero dial and the Heat glow, not from textures.
- Stage numbers (01–07) make the signal order readable at a glance.
- Ember is reserved for "macro / heat" so users learn: *ember = what Mix Ready is doing*.
