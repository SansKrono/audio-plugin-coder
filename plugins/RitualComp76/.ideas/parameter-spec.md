# Parameter Specification: RitualComp76

Control set mirrors the original 1176-style FET limiting amplifier front panel.

| ID | Name | Type | Range | Default | Unit | Description |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `input` | Input | Float | -∞ (off), -48.0 - 0.0 | -24.0 | dB | Input attenuator. Drives signal into the fixed-threshold FET stage. Higher = more compression and more saturation. Bottom of travel = mute (∞). |
| `output` | Output | Float | -∞ (off), -48.0 - 0.0 | -24.0 | dB | Output attenuator / makeup level. Bottom of travel = mute (∞). |
| `attack` | Attack | Float | 1.0 - 7.0 | 3.0 | step (1=slow, 7=fast) | Maps ~800 µs (1) → ~20 µs (7). Reverse-feel like original (clockwise = faster). |
| `release` | Release | Float | 1.0 - 7.0 | 5.0 | step (1=slow, 7=fast) | Maps ~1100 ms (1) → ~50 ms (7). Program-dependent recovery. |
| `ratio` | Ratio | Choice | Out, 4:1, 8:1, 12:1, 20:1, All | 4:1 | - | Ratio buttons. **Out** = compressor bypassed, amplifier color only. **All** = "all buttons in" smash mode. |
| `meter_mode` | Meter | Choice | GR, +8, +4, Off | GR | - | VU meter source: gain reduction, output level (+8/+4 dBu references), or off. Display only, no audio effect. |
| `bypass` | Power | Bool | Off/On | Off | - | Hard bypass (plugin equivalent of power switch). Standard host bypass. |

## Mapping Notes
- **Input/Output** taper: logarithmic, display labeled like original attenuator scale (∞, 48, 42, 36, 30, 24, 18, 12, 6, 0).
- **Attack/Release**: continuous (not stepped) between 1–7 markings; exponential time mapping.
- **Ratio UI**: four toggle buttons. Pressing multiple → `All`. None pressed → `Out`. Stored as single Choice param for automation.
- Total gain available (input + output stages) ≈ +45 dB, matching original amplifier headroom.
- All parameters automatable except `meter_mode` (automatable but non-audio).
