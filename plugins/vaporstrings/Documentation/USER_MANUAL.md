# VaporStrings — User Manual

**Version 0.9.0 (beta)** · Guitar → synth transformer · VST3 / AU / LV2 / Standalone

> Plug in a clean guitar. Play a synth from 1986's dreams.

VaporStrings listens to a clean (DI) guitar and rebuilds it as leads, pads, basses and smeared vaporwave textures. Three sound sources are blended, shaped by a synth voice, then sent through a six-stage vaporwave FX chain.

---

## Quick start

1. Put VaporStrings on a **mono or stereo guitar track** recorded or monitored **clean / DI** (no amp sim before it).
2. Play single notes and watch the **tracking display** (bottom-left): it should show the right note with a green confidence bar.
3. Step through the presets (◀ ▶ or click the name): *NEON LEAD*, *MALL AT 3AM*, *SUNSET BASS*, *PLASTIC CHOIR*, *BROKEN VHS*.
4. Turn **VAPOR** up for an instant clean → dissolved morph.

**Controls:** drag knobs vertically (Shift = fine), mouse wheel, double-click to reset. Click a flip to step forward (Shift-click or right-click = back). Hover a knob to see its value. Drag the bottom-right corner to resize (50 %–200 %, aspect locked).

---

## Signal flow

```
Guitar ─► INPUT (gain, gate, tracking) ─┬─► [A] Direct (+ Square-Up)
                                        ├─► [B] Octave layer (−1 / +1 / free shift)
                                        └─► [C] Synth (pitch-tracked oscillators)
            ─► VOICE (ladder filter + envelope) ─► SLOW ─► DRIVE ─► ENSEMBLE ─► WOBBLE ─► HAZE ─► ECHO ─► OUTPUT
```

### Input / tracking
The pitch tracker follows **one note at a time**. Chords still sound through the Direct and Octave layers, but the synth fades out and the display shows **CHORD? · MONO SYNTH**.
- **GATE** cuts string noise between notes and also tells the tracker when a note is playing.
- **SENS** — higher tracks softer, noisier playing (but risks false notes).
- **SPEED** — high = tight and fast (leads), low = stable and smooth (pads).
- **TRK HPF** — raise it if the low strings track an octave wrong; lower it for drop tunings.
- **PRIORITY** — *Last* (default), *Lowest* (robust on low strings), *Highest*.

### Sources
- **DIRECT** your guitar; **SQUARE** clips it into a near-square wave (classic guitar-synth trick).
- **OCTAVE** layer: **OCT −1**, **OCT +1** and a free **SHIFT** (±24 st). Works with chords.
- **SYNTH**: two oscillators (Saw / Square / Pulse / Tri / Sine) with octave, detune, mix and pulse width, **UNISON** up to 7 voices with **SPREAD** (detune + stereo), a sine **SUB** one octave down, and **GLIDE**.

### Voice
- Ladder **FILTER** (LP24 / LP12 / BP / HP) with cutoff, resonance, drive, and **ENV AMT** (± 6 octaves).
- **ENV** mode:
  - *Follow* — the synth follows your picking dynamics; Direct/Octave stay natural.
  - *ADSR* — every detected pick triggers a synth envelope; release starts when the gate closes. *Note: in ADSR everything stays silent until the first pick is detected — use Follow or Swell for volume swells.*
  - *Swell* — each note blooms in over the Attack time (string-machine pads).
- **PICK VEL** — how much picking strength affects level.

### FX (tabs)
| Tab | What it does |
|---|---|
| SLOW | Pitch the whole sound down/up (±12 st) with random **DRIFT** — "slowed + reverb". |
| DRIVE | Tape / Tube / Fuzz / Bitcrush with a ±6 dB tilt **TONE**. |
| ENSEMBLE | String-machine chorus: Chorus I, Chorus II, Ensemble. |
| WOBBLE | Tape **WOW**, **FLUTTER**, **AGE** (dark + hiss + saturation) and random **DROPOUTS**. |
| HAZE | Smear reverb: size, decay (0.2–30 s), pre-delay, **SMEAR**, damping, **REVERSE** swell, **FREEZE**. Tails ring out when switched off. |
| ECHO | Tape delay, free ms or tempo-synced divisions, feedback with tone filter and limiter. |
| MOD | Two LFOs (Sine/Tri/Saw/Square/S&H) → Cutoff, Pitch, PWM, Ens Depth, Haze Size or Volume. **SYNC** turns RATE into note values. |

### VAPOR macro
One knob from clean to dissolved. It adds Haze mix, tape wow and age, Ensemble depth, stretches attacks (up to ×40) with a swell bloom, shifts the balance toward the synth, and lowers Slowdown pitch (only while SLOW is on). Above 2 % it **engages HAZE and WOBBLE** (their tab LEDs light up). It never moves your other knobs or automation.

### Output
**WIDTH** (0 = mono, 2 = extra wide), **DRY/WET** against your clean guitar, **OUTPUT** gain, then a transparent safety clipper.

---

## Tips
- Synth notes wrong by an octave on the low strings → raise TRK HPF a little, set PRIORITY to *Lowest*, or lower SENS.
- Synth laggy → raise SPEED and lower GLIDE. Mushy chords → that's expected for the synth; use the Octave layer / Square-Up for polyphonic sounds.
- Latency: the plugin reports zero latency. Pitch shifting and tape effects add their own musical delay, like hardware pedals.

## Known limitations (0.9.0 beta)
- The synth source is monophonic by design.
- Octave-down has a slight warble on low notes (rotating-head shifter).
- Freeze is a very long hold (~0.6 dB/s fade), not infinite.
- Factory presets are a starting point and not yet fine-tuned.
- macOS and Linux builds are CI-built and untested on hardware; macOS builds are not signed/notarized.

---

## Parameter reference

| ID | Name | Range | Default |
|---|---|---|---|
| `in_gain` | Input Gain | -24 – 24 dB | 0 |
| `gate_thresh` | Gate Threshold | -90 – 0 dB | -60 |
| `track_hpf` | Tracking HPF | 40 – 400 Hz | 70 |
| `track_speed` | Tracking Speed | 0 – 1 0–1 | 0.6 |
| `track_sens` | Tracking Sensitivity | 0 – 1 0–1 | 0.5 |
| `note_priority` | Note Priority | Last / Lowest / Highest | Last |
| `glide` | Glide | 0 – 2000 ms | 30 |
| `src_direct_lvl` | Direct Level | -60 – 6 dB (−60 = off) | -6 |
| `direct_square` | Direct Square-Up | 0 – 1 0–1 | 0 |
| `src_oct_lvl` | Octave Layer Level | -60 – 6 dB (−60 = off) | -60 |
| `oct_down` | Octave Down Mix | 0 – 1 0–1 | 0.5 |
| `oct_up` | Octave Up Mix | 0 – 1 0–1 | 0 |
| `oct_shift` | Free Shift | -24 – 24 semitones | 0 |
| `src_synth_lvl` | Synth Level | -60 – 6 dB (−60 = off) | 0 |
| `osc1_wave` | Osc 1 Wave | Saw / Square / Pulse / Triangle / Sine | Saw |
| `osc1_oct` | Osc 1 Octave | -2 – 2 octaves | 0 |
| `osc2_wave` | Osc 2 Wave | Saw / Square / Pulse / Triangle / Sine | Square |
| `osc2_oct` | Osc 2 Octave | -2 – 2 octaves | 0 |
| `osc2_detune` | Osc 2 Detune | -100 – 100 cents | 7 |
| `osc_mix` | Osc 1/2 Mix | 0 – 1 0–1 | 0.5 |
| `pulse_width` | Pulse Width | 0.05 – 0.95 0–1 | 0.5 |
| `unison` | Unison Voices | 1 – 7 voices | 1 |
| `unison_spread` | Unison Spread | 0 – 1 0–1 | 0.3 |
| `sub_lvl` | Sub Osc Level | 0 – 1 0–1 | 0 |
| `flt_type` | Filter Type | LP24 / LP12 / BP / HP | LP24 |
| `flt_cutoff` | Cutoff | 20 – 20000 Hz | 4000 |
| `flt_res` | Resonance | 0 – 1 0–1 | 0.2 |
| `flt_drive` | Filter Drive | 0 – 1 0–1 | 0 |
| `flt_env_amt` | Filter Env Amount | -1 – 1 −1…+1 | 0.3 |
| `env_mode` | Envelope Mode | Follow / ADSR / Swell | Follow |
| `env_attack` | Attack | 0.5 – 5000 ms | 5 |
| `env_decay` | Decay | 5 – 5000 ms | 300 |
| `env_sustain` | Sustain | 0 – 1 0–1 | 0.8 |
| `env_release` | Release | 5 – 10000 ms | 400 |
| `vel_sens` | Pick Velocity Sens | 0 – 1 0–1 | 0.5 |
| `slow_on` | Slowdown On | Off / On | Off |
| `slow_pitch` | Slowdown Pitch | -12 – 12 semitones | -3 |
| `slow_drift` | Pitch Drift | 0 – 1 0–1 | 0.1 |
| `drive_on` | Drive On | Off / On | Off |
| `drive_mode` | Drive Mode | Tape / Tube / Fuzz / Bitcrush | Tape |
| `drive_amt` | Drive Amount | 0 – 1 0–1 | 0.3 |
| `drive_tone` | Drive Tone | 0 – 1 0–1 | 0.5 |
| `ens_on` | Ensemble On | Off / On | On |
| `ens_mode` | Ensemble Mode | Chorus I / Chorus II / Ensemble | Ensemble |
| `ens_rate` | Ensemble Rate | 0.05 – 8 Hz | 0.6 |
| `ens_depth` | Ensemble Depth | 0 – 1 0–1 | 0.5 |
| `ens_mix` | Ensemble Mix | 0 – 1 0–1 | 0.5 |
| `wob_on` | Wobble On | Off / On | Off |
| `wob_wow` | Wow | 0 – 1 0–1 | 0.3 |
| `wob_flutter` | Flutter | 0 – 1 0–1 | 0.2 |
| `wob_age` | Age | 0 – 1 0–1 | 0.2 |
| `wob_dropout` | Dropouts | 0 – 1 0–1 | 0 |
| `haze_on` | Haze On | Off / On | On |
| `haze_size` | Haze Size | 0 – 1 0–1 | 0.7 |
| `haze_decay` | Haze Decay | 0.2 – 30 s | 6 |
| `haze_predelay` | Haze Pre-Delay | 0 – 500 ms | 20 |
| `haze_mod` | Haze Smear Mod | 0 – 1 0–1 | 0.4 |
| `haze_damp` | Haze Damping | 0 – 1 0–1 | 0.4 |
| `haze_reverse` | Reverse Swell | 0 – 1 0–1 | 0 |
| `haze_freeze` | Freeze | Off / On | Off |
| `haze_mix` | Haze Mix | 0 – 1 0–1 | 0.35 |
| `echo_on` | Echo On | Off / On | Off |
| `echo_sync` | Echo Sync | Off / On | On |
| `echo_time` | Echo Time | 10 – 2000 ms | 375 |
| `echo_div` | Echo Division | 1/32 / 1/16T / 1/16 / 1/16D / 1/8T / 1/8 / 1/8D / 1/4T / 1/4 / 1/4D / 1/2 / 1/1 | 1/8D |
| `echo_fb` | Echo Feedback | 0 – 0.95 0–1 | 0.35 |
| `echo_tone` | Echo Tone | 0 – 1 0–1 | 0.5 |
| `echo_mix` | Echo Mix | 0 – 1 0–1 | 0.25 |
| `lfo1_shape` | LFO 1 Shape | Sine / Triangle / Saw / Square / S&H | Sine |
| `lfo1_rate` | LFO 1 Rate | 0.01 – 20 Hz | 1 |
| `lfo1_sync` | LFO 1 Sync | Off / On | Off |
| `lfo1_dest` | LFO 1 Destination | Cutoff / Pitch / PWM / Ens Depth / Haze Size / Volume | Cutoff |
| `lfo1_amt` | LFO 1 Amount | -1 – 1 −1…+1 | 0 |
| `lfo2_shape` | LFO 2 Shape | Sine / Triangle / Saw / Square / S&H | Triangle |
| `lfo2_rate` | LFO 2 Rate | 0.01 – 20 Hz | 0.25 |
| `lfo2_sync` | LFO 2 Sync | Off / On | Off |
| `lfo2_dest` | LFO 2 Destination | Cutoff / Pitch / PWM / Ens Depth / Haze Size / Volume | Pitch |
| `lfo2_amt` | LFO 2 Amount | -1 – 1 −1…+1 | 0 |
| `vapor` | VAPOR Macro | 0 – 1 0–1 | 0 |
| `width` | Stereo Width | 0 – 2 × | 1 |
| `dry_wet` | Dry / Wet | 0 – 1 0–1 | 1 |
| `out_gain` | Output Gain | -24 – 12 dB | 0 |

---

Built with Audio Plugin Coder (APC) · © 2026 Noizefield
