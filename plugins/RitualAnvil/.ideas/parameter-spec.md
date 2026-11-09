# Parameter Specification: RitualAnvil

Control set and ranges taken from the Ritual Suite POC (`RitualEngine/Source/Engine/ParameterManager.cpp`, `anvil_config.json`). IDs cleaned of the POC's `anvil_` prefix (no longer sharing one global parameter namespace); POC ID kept for reference.

| ID | Name | Type | Range | Default | Unit | POC ID | Description |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `mix_ready` | Mix Ready | Float | 0.0 - 100.0 | 0.0 | % | `mix_ready` | Macro. Moves HPF, EQ, transients, punch, heat and width together (see brief). |
| `input_gain` | Input | Float | -12.0 - 12.0 | 0.0 | dB | `input_gain` | Input trim before the chain. |
| `hpf` | HPF | Float | 20.0 - 100.0 | 40.0 | Hz | `anvil_hpf` | High-pass cutoff, Q 0.707. Log taper. |
| `transient_attack` | Attack | Float | -12.0 - 12.0 | 0.0 | dB | `transient_attack` | Transient boost/cut, low+mid bands (<20 kHz split; split at 300 Hz). |
| `transient_sustain` | Sustain | Float | -12.0 - 6.0 | 0.0 | dB | `transient_sustain` | Body/sustain boost/cut, low+mid bands. |
| `eq_kick` | Kick | Float | -12.0 - 12.0 | 0.0 | dB | `eq_kick` | Peak @ 60 Hz, Q 1.0. |
| `eq_box` | Box | Float | -12.0 - 12.0 | 0.0 | dB | `eq_box` | Peak @ 200 Hz, Q 1.0. |
| `eq_snare` | Snare | Float | -12.0 - 12.0 | 0.0 | dB | `eq_snare` | Peak @ 3 kHz, Q 1.0. |
| `eq_presence` | Presence | Float | -12.0 - 12.0 | 0.0 | dB | `eq_pres` | Peak @ 8 kHz, Q 0.707. |
| `eq_air` | Air | Float | -12.0 - 12.0 | 0.0 | dB | `eq_air` | Peak @ 12 kHz, Q 0.5. |
| `punch` | Punch | Float | 0.0 - 100.0 | 0.0 | % | `punch` | Parallel comp wet blend; 100% = 50% wet. Fixed comp: −20 dB, 8:1, 3 ms / 150 ms. |
| `heat` | Heat | Float | 0.0 - 20.0 | 0.0 | dB | `heat` | Drive into 3-band saturation (200 Hz / 5 kHz; hard / soft / tube). |
| `width` | Width | Float | 50.0 - 150.0 | 100.0 | % | `anvil_width` | M/S side gain; side HPF fixed 200 Hz. |
| `ceiling` | Ceiling | Float | -1.0 - -0.1 | -0.3 | dBTP | `anvil_ceiling` | True-peak limiter ceiling, release 150 ms. |
| `bypass` | Bypass | Bool | Off/On | Off | - | (new) | Host bypass, latency-compensated. |

## Fixed Internal Values (not exposed in v1)
| Stage | Value |
| :--- | :--- |
| Transient split | 300 Hz / 20 kHz; highs untouched |
| Transient envelopes | fast 1 ms / 30 ms, slow 20 ms / 200 ms |
| Parallel comp | thresh −20 dB, ratio 8:1, attack 3 ms, release 150 ms |
| Heat split | 200 Hz / 5 kHz |
| Width side HPF | 200 Hz |
| Limiter release | 150 ms |

## Mix Ready Targets (POC, at t = mix_ready/100)
| Target | Law |
| :--- | :--- |
| HPF | 30 + 20·t Hz |
| Kick / Box / Snare / Presence / Air | +4t / −3t / +3t / +2t / +4t dB |
| Punch blend | clamp((t − 0.4)·0.5/0.6, 0, 0.5) |
| Transient attack | t > 0.5 ? (t − 0.5)·8 dB : 0 |
| Transient sustain | t < 0.5 ? (t − 0.5)·6 dB : 0 |
| Heat drive | t > 0.6 ? (t − 0.6)·15 dB : 0 |
| Width | 1 + clamp((t − 0.5)·2, 0, 1)·0.2 |

**Note:** POC replaced manual values with these when `mix_ready > 0`. v1 layers them as offsets on manual values (clamped to each param's range). Final law confirmed in `/plan`.

## Mapping Notes
- All parameters automatable and smoothed (~20 ms gains, ~50 ms frequencies).
- Frequency params use log skew; dB params linear.

**Design-phase amendment (v1 law, see architecture.md):** HPF floor = `max(manual, 20 + 30t)` and sustain dip = triangle (−3 dB at t = 0.25, 0 at t = 0 and 0.5) so the macro has no jump at t = 0⁺. POC formulas above kept for reference.
