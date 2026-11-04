# VaporStrings — Parameter Specification

All parameters are host-automatable unless noted. Choice parameters list their options in the Range column.

## Input / Tracking
| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `in_gain` | Input Gain | Float | -24.0 – +24.0 | 0.0 | dB |
| `gate_thresh` | Gate Threshold | Float | -90.0 – 0.0 | -60.0 | dB |
| `track_hpf` | Tracking HPF | Float | 40.0 – 400.0 | 70.0 | Hz |
| `track_speed` | Tracking Speed | Float | 0.0 – 1.0 | 0.6 | % |
| `track_sens` | Tracking Sensitivity | Float | 0.0 – 1.0 | 0.5 | % |
| `note_priority` | Note Priority | Choice | Last / Lowest / Highest | Last | — |
| `glide` | Glide | Float | 0.0 – 2000.0 | 30.0 | ms |

## Sources
| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `src_direct_lvl` | Direct Level | Float | -inf – +6.0 | -6.0 | dB |
| `direct_square` | Direct Square-Up | Float | 0.0 – 1.0 | 0.0 | % |
| `src_oct_lvl` | Octave Layer Level | Float | -inf – +6.0 | -inf | dB |
| `oct_down` | Octave Down Mix | Float | 0.0 – 1.0 | 0.5 | % |
| `oct_up` | Octave Up Mix | Float | 0.0 – 1.0 | 0.0 | % |
| `oct_shift` | Free Shift | Float | -24.0 – +24.0 | 0.0 | st |
| `src_synth_lvl` | Synth Level | Float | -inf – +6.0 | 0.0 | dB |
| `osc1_wave` | Osc 1 Wave | Choice | Saw / Square / Pulse / Triangle / Sine | Saw | — |
| `osc1_oct` | Osc 1 Octave | Int | -2 – +2 | 0 | oct |
| `osc2_wave` | Osc 2 Wave | Choice | Saw / Square / Pulse / Triangle / Sine | Square | — |
| `osc2_oct` | Osc 2 Octave | Int | -2 – +2 | 0 | oct |
| `osc2_detune` | Osc 2 Detune | Float | -100.0 – +100.0 | 7.0 | ct |
| `osc_mix` | Osc 1/2 Mix | Float | 0.0 – 1.0 | 0.5 | % |
| `pulse_width` | Pulse Width | Float | 0.05 – 0.95 | 0.5 | — |
| `unison` | Unison Voices | Int | 1 – 7 | 1 | voices |
| `unison_spread` | Unison Spread | Float | 0.0 – 1.0 | 0.3 | % |
| `sub_lvl` | Sub Osc Level | Float | 0.0 – 1.0 | 0.0 | % |

## Synth Voice
| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `flt_type` | Filter Type | Choice | LP24 / LP12 / BP / HP | LP24 | — |
| `flt_cutoff` | Cutoff | Float | 20.0 – 20000.0 (log) | 4000.0 | Hz |
| `flt_res` | Resonance | Float | 0.0 – 1.0 | 0.2 | % |
| `flt_drive` | Filter Drive | Float | 0.0 – 1.0 | 0.0 | % |
| `flt_env_amt` | Filter Env Amount | Float | -1.0 – +1.0 | 0.3 | — |
| `env_mode` | Envelope Mode | Choice | Follow / ADSR / Swell | Follow | — |
| `env_attack` | Attack | Float | 0.5 – 5000.0 (log) | 5.0 | ms |
| `env_decay` | Decay | Float | 5.0 – 5000.0 (log) | 300.0 | ms |
| `env_sustain` | Sustain | Float | 0.0 – 1.0 | 0.8 | % |
| `env_release` | Release | Float | 5.0 – 10000.0 (log) | 400.0 | ms |
| `vel_sens` | Pick Velocity Sens | Float | 0.0 – 1.0 | 0.5 | % |

## FX Chain
| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `slow_on` | Slowdown On | Bool | Off / On | Off | — |
| `slow_pitch` | Slowdown Pitch | Float | -12.0 – +12.0 | -3.0 | st |
| `slow_drift` | Pitch Drift | Float | 0.0 – 1.0 | 0.1 | % |
| `drive_on` | Drive On | Bool | Off / On | Off | — |
| `drive_mode` | Drive Mode | Choice | Tape / Tube / Fuzz / Bitcrush | Tape | — |
| `drive_amt` | Drive Amount | Float | 0.0 – 1.0 | 0.3 | % |
| `drive_tone` | Drive Tone | Float | 0.0 – 1.0 | 0.5 | % |
| `ens_on` | Ensemble On | Bool | Off / On | On | — |
| `ens_mode` | Ensemble Mode | Choice | Chorus I / Chorus II / Ensemble | Ensemble | — |
| `ens_rate` | Ensemble Rate | Float | 0.05 – 8.0 | 0.6 | Hz |
| `ens_depth` | Ensemble Depth | Float | 0.0 – 1.0 | 0.5 | % |
| `ens_mix` | Ensemble Mix | Float | 0.0 – 1.0 | 0.5 | % |
| `wob_on` | Wobble On | Bool | Off / On | Off | — |
| `wob_wow` | Wow | Float | 0.0 – 1.0 | 0.3 | % |
| `wob_flutter` | Flutter | Float | 0.0 – 1.0 | 0.2 | % |
| `wob_age` | Age | Float | 0.0 – 1.0 | 0.2 | % |
| `wob_dropout` | Dropouts | Float | 0.0 – 1.0 | 0.0 | % |
| `haze_on` | Haze On | Bool | Off / On | On | — |
| `haze_size` | Haze Size | Float | 0.0 – 1.0 | 0.7 | % |
| `haze_decay` | Haze Decay | Float | 0.2 – 30.0 (log) | 6.0 | s |
| `haze_predelay` | Haze Pre-Delay | Float | 0.0 – 500.0 | 20.0 | ms |
| `haze_mod` | Haze Smear Mod | Float | 0.0 – 1.0 | 0.4 | % |
| `haze_damp` | Haze Damping | Float | 0.0 – 1.0 | 0.4 | % |
| `haze_reverse` | Reverse Swell | Float | 0.0 – 1.0 | 0.0 | % |
| `haze_freeze` | Freeze | Bool | Off / On | Off | — |
| `haze_mix` | Haze Mix | Float | 0.0 – 1.0 | 0.35 | % |
| `echo_on` | Echo On | Bool | Off / On | Off | — |
| `echo_sync` | Echo Sync | Bool | Off / On | On | — |
| `echo_time` | Echo Time | Float | 10.0 – 2000.0 | 375.0 | ms |
| `echo_div` | Echo Division | Choice | 1/32 … 1/1 (incl. dotted/triplet) | 1/8D | — |
| `echo_fb` | Echo Feedback | Float | 0.0 – 0.95 | 0.35 | % |
| `echo_tone` | Echo Tone | Float | 0.0 – 1.0 | 0.5 | % |
| `echo_mix` | Echo Mix | Float | 0.0 – 1.0 | 0.25 | % |

## Modulation
| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `lfo1_shape` | LFO 1 Shape | Choice | Sine / Triangle / Saw / Square / S&H | Sine | — |
| `lfo1_rate` | LFO 1 Rate | Float | 0.01 – 20.0 (log) | 1.0 | Hz |
| `lfo1_sync` | LFO 1 Sync | Bool | Off / On | Off | — |
| `lfo1_dest` | LFO 1 Destination | Choice | Cutoff / Pitch / PWM / Ens Depth / Haze Size / Volume | Cutoff | — |
| `lfo1_amt` | LFO 1 Amount | Float | -1.0 – +1.0 | 0.0 | — |
| `lfo2_shape` | LFO 2 Shape | Choice | Sine / Triangle / Saw / Square / S&H | Triangle | — |
| `lfo2_rate` | LFO 2 Rate | Float | 0.01 – 20.0 (log) | 0.25 | Hz |
| `lfo2_sync` | LFO 2 Sync | Bool | Off / On | Off | — |
| `lfo2_dest` | LFO 2 Destination | Choice | Cutoff / Pitch / PWM / Ens Depth / Haze Size / Volume | Pitch | — |
| `lfo2_amt` | LFO 2 Amount | Float | -1.0 – +1.0 | 0.0 | — |

## Global
| ID | Name | Type | Range | Default | Unit |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `vapor` | VAPOR Macro | Float | 0.0 – 1.0 | 0.0 | % |
| `width` | Stereo Width | Float | 0.0 – 2.0 | 1.0 | x |
| `dry_wet` | Dry / Wet | Float | 0.0 – 1.0 | 1.0 | % |
| `out_gain` | Output Gain | Float | -24.0 – +12.0 | 0.0 | dB |

## Read-only meters (UI only, not automatable)
| ID | Name | Description |
| :--- | :--- | :--- |
| `m_note` | Detected Note | Current tracked pitch (note + cents) |
| `m_conf` | Tracking Confidence | 0–1; low values warn on chords/noise |
| `m_env` | Envelope Level | Envelope follower output |
| `m_in` / `m_out` | I/O Meters | Peak/RMS levels |

**Total automatable parameters:** 82 (complexity is high — plan phase should consider phasing implementation: Sources + Voice first, then FX chain, then modulation.)

### VAPOR macro mapping (default)
| Target | 0.0 → 1.0 |
| :--- | :--- |
| `haze_mix` | +0 → +0.5 |
| `wob_wow` / `wob_age` | +0 → +0.5 |
| `slow_pitch` | 0 → −5 st (when Slowdown on) |
| `env_attack` | ×1 → ×40 (swell) |
| `src_synth_lvl` blend | rises toward synth source |
| `ens_depth` | +0 → +0.4 |
