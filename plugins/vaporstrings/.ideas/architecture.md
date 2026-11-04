# DSP Architecture Specification — VaporStrings

Guitar → synth transformer. Audio effect, mono-in or stereo-in (input summed to mono), stereo out.
Sample rates 44.1–192 kHz; all buffers are allocated in `prepareToPlay`. Nothing is allocated on the audio thread.

## Core Components

### Input stage
- **Input Gain:** `SmoothedValue` gain applied to the mono sum of the input.
- **Noise Gate:** peak detector with 1 ms attack, 80 ms release and a soft gain ramp. Its open/closed state is also used as the *note gate* for the tracker.
- **Tracking HPF:** 2nd-order Butterworth HPF (`juce::dsp::IIR`) plus a fixed 1.2 kHz LPF. This feeds the **analysis path only** and never touches the audio path.

### Analysis (control signals)
- **Pitch Tracker (YIN):**
  - Analysis signal is decimated ×4 (to about 12 kHz) with a half-band chain.
  - 512-sample window at the decimated rate, about 42 ms. This covers the low E (82 Hz) with more than 3 periods.
  - Hop of 64 decimated samples, about 5 ms update rate.
  - Parabolic interpolation on the difference minimum.
  - Outputs: `f0` (Hz), `confidence` (1 − d′min), `voiced` (bool).
  - `track_sens` sets the YIN threshold (0.05–0.35). `track_speed` sets the median-filter length (1–7 frames) and the hysteresis on note changes.
  - **Octave-error guard:** reject jumps of exactly ×2/×0.5 unless they persist for 2 or more frames.
  - **Hold:** the last good pitch is kept while `voiced` drops (so release tails keep their pitch).
- **Note priority:** the tracker reports one pitch only. `note_priority` decides which pitch it prefers when there are competing candidates:
  - *Lowest*: the deepest local minimum below threshold.
  - *Highest*: the shallowest.
  - *Last*: the most recent change.
- **Onset / Velocity Detector:**
  - Spectral-flux-lite: the difference of fast (1 ms) and slow (30 ms) envelopes on the HPF'd signal.
  - Fires `noteOn(velocity)` when it crosses an adaptive threshold, with a 40 ms retrigger lockout.
  - Velocity = peak of the fast envelope in the 5 ms after the onset, scaled by `vel_sens`.
- **Envelope Follower:** peak follower on the audio path, 2 ms attack and 120 ms release. Used as the "Follow" amp envelope, as a filter mod source, and for the `m_env` meter.

### Sound Sources
- **[A] Direct:**
  - Dry guitar, then *Square-Up*: pre-emphasis, then gain (`direct_square`: 0 → +48 dB), then a tanh → hard-clip blend, then de-emphasis and level-compensation.
  - Runs ×2 oversampled (`juce::dsp::Oversampling`, polyphase IIR).
- **[B] Octave Layer:** three *rotating-head delay-line pitch shifters*. Each is 2 taps with a Hann crossfade and a 40 ms grain, so they are causal and work on chords.
  - Shifter 1: fixed −12 st (`oct_down`)
  - Shifter 2: fixed +12 st (`oct_up`)
  - Shifter 3: free shift (`oct_shift`, −24…+24 st). It is skipped when `oct_shift` = 0.
  - Summed, then `src_oct_lvl`.
- **[C] Synth Oscillators** (monophonic, driven by the tracker):
  - Frequency path: `f0` → glide (a one-pole in **log-frequency**, time = `glide`) → per-osc octave/detune → LFO pitch mod.
  - **Osc 1 / Osc 2:** PolyBLEP saw, square and pulse (PWM). The triangle is an integrated PolyBLEP square, plus a sine.
  - **Unison:** up to 7 voices per osc, detune spread ±`unison_spread`×25 ct, panned across the stereo field. This is the only stereo element before the FX chain.
  - **Sub osc:** sine or square at −1 oct relative to Osc 1.
  - **Phase reset:** none (free-running), to avoid clicks.
  - **Source level:** gated by the amp envelope (see Voice) and multiplied by `src_synth_lvl`.

### Synth Voice (applies to the mixed sources)
- **Source Mixer:** A + B (mono, duplicated L/R) + C (stereo).
- **Filter:** `juce::dsp::LadderFilter` (modes LPF12 / LPF24 / BPF12 / HPF12, with built-in drive), stereo.
  - Cutoff mod is summed in octaves: `flt_env_amt` × envelope × 6 oct, plus LFO.
  - Cutoff is smoothed at control rate.
- **Amp Envelope** (`env_mode`):
  - *Follow:* the envelope follower output, normalised by velocity.
  - *ADSR:* `juce::ADSR` triggered by the onset detector. Gate-off happens when the noise gate closes.
  - *Swell:* follower × a re-triggerable attack ramp (`env_attack`, used for slow string swells).
- The **same** envelope drives the filter env mod.
- **Gating scope:** the envelope gates **source C** fully. For A and B it only shapes the level when mode ≠ Follow, so the dry/octave layers keep their natural guitar dynamics in Follow mode.

### FX Chain (fixed order, stereo, each with a bypass using a 10 ms crossfade)
1. **Slowdown:** stereo rotating-head pitch shifter (60 ms grain, lower artifacts for pads).
   - Pitch = `slow_pitch` + drift, where drift is a smoothed random LFO at 0.2 Hz × `slow_drift` × 30 ct.
2. **Drive** (×2 oversampled except Bitcrush):
   - *Tape:* tanh with a gentle pre/de-emphasis tilt
   - *Tube:* asymmetric `x / (1 + |x|)` plus a bias offset with DC-block
   - *Fuzz:* hard clip plus a pre-gain bump at 800 Hz
   - *Bitcrush:* bit depth 16→3 and a sample-and-hold rate reduction
   - `drive_tone` = a tilt EQ after the drive. The output is level-compensated.
3. **Ensemble** (string-machine BBD model):
   - Three modulated delay lines (base 5/7/10 ms). Each LFO is the sum of a slow sine (`ens_rate`) and a fast sine (`ens_rate` × 9.5, 0.3 depth), at 0°/120°/240°.
   - Modes:
     - *Chorus I:* 1 line per side
     - *Chorus II:* 2 lines
     - *Ensemble:* 3 lines with cross-panned outputs
   - Gentle BBD LPF (≈9 kHz) in the wet path. `ens_mix` sets the dry/wet.
4. **Tape Wobble / Age:**
   - A modulated delay line (base 8 ms):
     - **Wow:** smoothed random walk at 0.3–1.5 Hz, ±3 ms × `wob_wow`
     - **Flutter:** 6–12 Hz sine plus jitter, ±0.3 ms × `wob_flutter`
   - **Age:** LPF from 20 kHz down to 3.5 kHz, a light saturation, and pink-noise hiss at −70 to −40 dB.
   - **Dropouts:** a Poisson-timed gain dip (up to −24 dB, 30–200 ms, smoothed edges). The rate scales with `wob_dropout`.
5. **Haze** (smear reverb):
   - **Pre-delay:** 0–500 ms.
   - **Reverse Swell:** an optional reverse-grain buffer on the reverb **input** (300 ms chunks played backwards with a Hann window), blended in by `haze_reverse`.
   - **Input diffusion:** 4 series allpasses.
   - **Main reverb:** an 8-line **FDN** with a Householder matrix.
     - Delay lengths are mutually prime, 30–150 ms, scaled by `haze_size`.
     - Each line is modulated (`haze_mod`, 0.1–1.2 Hz, ±1 ms) for the smear.
     - Per-line one-pole damping (`haze_damp`). Feedback gain is computed from `haze_decay` (RT60).
   - **Freeze:** feedback → 1.0, damping off, input muted (with a ramp).
   - `haze_mix` sets the dry/wet.
6. **Echo:**
   - Stereo delay line, max 2.5 s.
   - Time comes from `echo_time`, or from `echo_div` × host BPM (`AudioPlayHead`) when `echo_sync` is on. The time is smoothed to avoid zipper noise.
   - Feedback loop: tone (LPF 1.5–12 kHz and HPF 80–400 Hz, mapped from `echo_tone`), a soft-clip limiter, and a slight wow taken from the wobble LFO.
   - `echo_mix` sets the dry/wet.

### Modulation & Macro (control rate: every 32 samples)
- **LFO 1 / LFO 2:** sine, triangle, saw, square and S&H. Rate is free or synced to the host BPM, and the phase is synced to the PPQ position when synced.
  - Each has one destination: Cutoff (±4 oct), Pitch (±1 st), PWM (±0.4), Ens Depth (±0.5), Haze Size (±0.3), or Volume (tremolo).
- **VAPOR Macro:** applied as additive or multiplicative offsets to the targets listed in the parameter spec, before clamping. The user's parameter values are never modified, so automation stays clean.

### Output
- **Width:** M/S (mid/side) scaling at the end of the chain.
- **Dry/Wet:** the latency-free dry guitar (mono, duplicated) against the processed chain, with an equal-power crossfade.
- **Output Gain**, then a safety soft-clip at 0 dBFS (true-peak is not required).

## Processing Chain

```
Input (L+R → mono) → Input Gain → Noise Gate ─┬─────────────────────────────────────────────────────────────┐ (dry)
                                              ├─► Tracking HPF/LPF → ↓4 → YIN Pitch Tracker ─► f0, conf ────┐ │
                                              ├─► Onset/Velocity Detector ─► noteOn/vel ───────────────┐ │ │
                                              ├─► Envelope Follower ─► env ──────────────────────┐   │ │ │
                                              │                                                  ▼   ▼ ▼ │
                                              ├─► [A] Square-Up (×2 OS) ──────────┐       Amp/Filter Env  │
                                              ├─► [B] Octave Shifters (−12/+12/free) ─┤             │       │
                                              └─► [C] Osc1+Osc2+Unison+Sub (f0+glide) ─┴─► Source Mix ─► Ladder Filter ─► Amp Env
                                                                                                                          │
     ┌────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
     ▼
  Slowdown → Drive (×2 OS) → Ensemble → Wobble/Age → Haze (pre-delay → reverse → diffusers → FDN) → Echo → Width → Dry/Wet ← (dry)
                                                                                                                  → Out Gain → Safety Clip → Output
   LFO1/LFO2 + VAPOR macro ──► control-rate offsets to Cutoff / Pitch / PWM / Ensemble / Haze / Volume / etc.
```

## Parameter Mapping

| Parameter | Component | Function | Range |
|-----------|-----------|----------|-------|
| `in_gain` | Input Gain | Pre-gain on mono sum | −24…+24 dB |
| `gate_thresh` | Noise Gate | Gate open threshold + note gate | −90…0 dB |
| `track_hpf` | Tracking HPF | Analysis-path high-pass | 40–400 Hz |
| `track_speed` | Pitch Tracker | Median length / hysteresis (fast ↔ stable) | 0–1 |
| `track_sens` | Pitch Tracker / Onset | YIN threshold, onset threshold | 0–1 |
| `note_priority` | Pitch Tracker | Candidate selection | Last/Lowest/Highest |
| `glide` | Osc frequency path | Log-freq slew time | 0–2000 ms |
| `src_direct_lvl` | Source Mix | Level of [A] | −inf…+6 dB |
| `direct_square` | Square-Up | Clip drive + blend | 0–1 |
| `src_oct_lvl` | Source Mix | Level of [B] | −inf…+6 dB |
| `oct_down` / `oct_up` | Octave Shifters | Per-shifter mix | 0–1 |
| `oct_shift` | Free Shifter | Pitch ratio | −24…+24 st |
| `src_synth_lvl` | Source Mix | Level of [C] | −inf…+6 dB |
| `osc1_wave` / `osc2_wave` | Oscillators | Waveform select | 5 choices |
| `osc1_oct` / `osc2_oct` | Oscillators | Octave offset | −2…+2 |
| `osc2_detune` | Osc 2 | Fine detune | ±100 ct |
| `osc_mix` | Oscillators | Osc 1/2 crossfade | 0–1 |
| `pulse_width` | Oscillators | Pulse duty cycle | 0.05–0.95 |
| `unison` / `unison_spread` | Unison | Voice count, detune + pan spread | 1–7 / 0–1 |
| `sub_lvl` | Sub Osc | Sub level | 0–1 |
| `flt_type` | Ladder Filter | Mode | LP24/LP12/BP/HP |
| `flt_cutoff` / `flt_res` / `flt_drive` | Ladder Filter | Cutoff / Q / drive | 20 Hz–20 kHz / 0–1 / 0–1 |
| `flt_env_amt` | Filter Mod | Envelope → cutoff (±6 oct) | −1…+1 |
| `env_mode` | Amp Envelope | Follow / ADSR / Swell | choice |
| `env_attack`…`env_release` | Amp Envelope | ADSR stages (attack also = swell time) | ms / % |
| `vel_sens` | Onset Detector | Velocity scaling | 0–1 |
| `slow_on` / `slow_pitch` / `slow_drift` | Slowdown | Bypass / shift / random drift | −12…+12 st |
| `drive_on` / `drive_mode` / `drive_amt` / `drive_tone` | Drive | Bypass / algorithm / gain / tilt | — |
| `ens_on` / `ens_mode` / `ens_rate` / `ens_depth` / `ens_mix` | Ensemble | Bypass / line config / LFO / mod depth / wet | 0.05–8 Hz |
| `wob_on` / `wob_wow` / `wob_flutter` / `wob_age` / `wob_dropout` | Wobble | Bypass / slow mod / fast mod / LPF+hiss+sat / dropout rate | 0–1 |
| `haze_on` / `haze_size` / `haze_decay` / `haze_predelay` | Haze | Bypass / FDN length scale / RT60 / pre-delay | 0.2–30 s |
| `haze_mod` / `haze_damp` / `haze_reverse` / `haze_freeze` / `haze_mix` | Haze | Smear mod / damping / reverse blend / freeze / wet | — |
| `echo_on` / `echo_sync` / `echo_time` / `echo_div` | Echo | Bypass / tempo-sync / free time / note division | 10–2000 ms |
| `echo_fb` / `echo_tone` / `echo_mix` | Echo | Feedback / loop filter / wet | 0–0.95 |
| `lfo{1,2}_shape` / `_rate` / `_sync` / `_dest` / `_amt` | LFOs | Shape / rate / sync / routing / depth | 0.01–20 Hz |
| `vapor` | Macro | Multi-target morph offsets | 0–1 |
| `width` | Output M/S | Side gain | 0–2× |
| `dry_wet` | Output | Equal-power dry/processed blend | 0–1 |
| `out_gain` | Output | Final gain | −24…+12 dB |
| `m_note`, `m_conf`, `m_env`, `m_in`, `m_out` | Meters | Atomics → WebView at 30 Hz (not APVTS params) | read-only |

## Latency
- **Reported latency: 0 samples.**
  - The tracker runs on past audio only, and the pitch shifters are causal.
  - Their inherent grain delay (about 20–30 ms) is part of the sound, as on hardware octavers.
- The dry path is not delayed, so a 100 % `dry_wet` stays zero-latency.
- **Pitch → osc lag:** about 5–15 ms after the note settles. This is acceptable for guitar synths, and the attack transient is covered by sources A/B.

## Complexity Assessment

**Score: 4 (Expert)**

**Rationale:**
- **Real-time analysis:** monophonic YIN pitch tracking on guitar, with octave-error handling, hold, and onset/velocity detection that feeds a synthesis engine. Guitar tracking is the riskiest part.
- **Synthesis engine:** PolyBLEP oscillators with unison, glide, an envelope system with three modes, and a ladder filter.
- **Many DSP algorithms:**
  - 4 pitch-shifter instances
  - an oversampled waveshaper ×2
  - a BBD ensemble
  - a tape model
  - an 8-line modulated FDN with reverse and freeze
  - a tempo-synced echo
  - 2 LFOs and a macro layer
- **Scale:** 82 automatable parameters plus 5 meters streamed to a WebView UI.
- **Why not 5:** no ML, and no polyphonic pitch detection or complex physical modelling. Polyphony is handled by the causal shifter approach rather than polyphonic tracking.
