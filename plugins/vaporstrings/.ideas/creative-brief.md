# VaporStrings — Creative Brief

## Hook
**Plug in a clean guitar. Play a synth from 1986's dreams.**
VaporStrings is a guitar-to-synth transformer that listens to your strings and rebuilds them as neon leads, warped pads, sub-heavy basses and smeared, slowed-down vaporwave textures — with deep control over *how* the plugin hears, rebuilds and degrades your playing.

## Description

### Signal concept
VaporStrings is an **audio effect** (insert on a clean/DI guitar track). It runs the guitar through three parallel *sound sources*, blends them, shapes them like a synth voice, then sends them through a vaporwave FX chain.

```
Guitar In ─► Input Conditioner ─┬─► [A] Direct / Processed DI ─────────┐
 (gate, comp, HPF, tracking EQ)  ├─► [B] Octave / Pitch-Shift layer ─────┼─► Source Mix ─► Synth Voice ─► FX Chain ─► Out
                                 └─► [C] Pitch-Tracked Oscillators ──────┘   (filter + env        (ensemble, wobble,
                                         (mono YIN tracker → 2 osc + sub)     follower/ADSR)        drive, smear, delay)
```

### 1. Input Conditioner — "how the plugin hears you"
- Noise gate + sensitivity so the tracker and envelope follower react cleanly.
- Pre-tracking high-pass / tone filter to isolate the fundamental.
- **Tracking controls:** response speed vs. stability, glide, pitch-detection threshold, mono-note priority (last / lowest / highest). These are core to customisation: fast tracking = tight leads, slow tracking = gooey, portamento-heavy pads.

### 2. Sound sources
- **[A] Direct:** the guitar itself, optionally "squared up" (wave-folder / square fuzz — classic guitar-synth trick) so it behaves like a polyphonic oscillator.
- **[B] Octave layer:** polyphonic-friendly pitch shifter: sub-octave, octave-up, and a free ±24 st shifter for 5ths/harmony; adds shimmer and weight while staying polyphonic.
- **[C] Synth oscillators:** monophonic pitch tracker drives two oscillators (saw / square / triangle / sine / PWM) with detune, octave, unison, plus a sub-osc. Velocity comes from pick attack. This is where true leads and basses come from.

### 3. Synth Voice
- Multimode filter (LP24 / LP12 / BP / HP) with resonance and drive.
- **Envelope follower → filter** (auto-wah/"touch" sweeps) or a **triggered ADSR** fired by pick transients — selectable, so pads can swell slowly ("bowed" volume-swell mode) while leads snap.
- Amp envelope: follow-the-guitar vs. ADSR, with a **Swell** mode that auto-fades in every note for string-machine pads.

### 4. Vaporwave FX chain (fixed order, each with bypass)
1. **Pitch / Slowdown** — global "slowed + reverb" pitch drop (–12 st to +12 st), with optional tape-style formant-free drop and *drift*.
2. **Drive** — tape saturation, tube, fuzz, bitcrush modes.
3. **Ensemble** — classic string-machine triple-chorus / BBD ensemble, rate + depth + mode (Chorus I / II / Ensemble).
4. **Tape Wobble / Age** — wow, flutter, hiss, high-cut "age", random dropouts.
5. **Haze** — huge smear reverb (diffuse, modulated, with freeze and pre-delay), plus optional reverse-swell character.
6. **Echo** — tempo-syncable tape delay with feedback and filter in the loop.

### 5. Modulation
- 2 LFOs (shape, rate, tempo sync) assignable to filter cutoff, pitch, PWM, ensemble depth, haze size.
- Envelope follower usable as a mod source.
- **Macro "VAPOR"** knob: one-knob morph from clean guitar → fully dissolved pad (raises haze, wobble, slowdown, swell, source C mix).

### Presets (character targets)
- *Neon Lead* — tight tracking, saw + square, glide, LP filter, ensemble light, echo.
- *Mall at 3AM* — swell pad, slowed –3 st, heavy ensemble, Haze 80%, tape age.
- *Sunset Bass* — sub-osc + octave down, LP12, short decay, drive.
- *Plastic Choir* — squared-up direct + octave up, BP filter, ensemble wide.
- *Broken VHS* — heavy wobble, bitcrush, dropouts, reverse haze.

### Character & constraints
- Target: **clean, single-coil or humbucker DI**. Polyphonic playing works via sources A + B; source C is monophonic by design (tracking indicator in UI warns on chords).
- Latency target: ≤ 10 ms for tracking (report latency to host for the pitch shifter).
- Stereo output; mono input supported.

### Visual aesthetic — Vaporwave Neon
Pink → cyan sunset gradient, retro wireframe sun-grid horizon, chrome / glitch display type with Japanese sub-labels, CRT scanlines, glowing neon knob rings. A live **tracking display** (detected note + confidence) and a scrolling **"waveform on the horizon"** visualiser at the top. Sections laid out left-to-right in signal order: INPUT · SOURCES · VOICE · FX · MOD, with the big VAPOR macro dial in the centre.
