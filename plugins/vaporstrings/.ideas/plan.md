# Implementation Plan — VaporStrings

## Complexity Score: 4

## UI Framework
**Decision: WebView** (`juce::WebBrowserComponent`, WebView2 on Windows).
- **Rationale:** the vaporwave neon look needs things native code makes hard: gradients, a sun-grid horizon, CRT scanlines, neon glow and glitch type. The plugin also needs rich live visualisation (the tracking display and a horizon waveform), and 82 parameters split into sections/tabs are easier to lay out in HTML/CSS.
- **Other reasons:** WebView is the project default, and Visage is disabled in `apc.config.json`.

## Formats
VST3 + Standalone. CLAP is **not** enabled (user choice).

## Implementation Strategy: Phased

DSP lives in small header/source classes under `Source/dsp/`. Each class has `prepare` / `reset` / `process`, and none allocates after `prepare`. `PluginProcessor` owns the APVTS and orchestrates the classes.

Each phase must build, load in the Standalone, and pass a quick listening check before the next phase starts.

**Phase 2.1.1: Skeleton & Signal Path**
- [ ] CMake target, APVTS layout with **all 82 parameters** (IDs exactly as in `parameter-spec.md`)
- [ ] Input gain, mono sum, noise gate, dry path, `dry_wet`, `out_gain`, safety clip
- [ ] Source [A] Direct with `src_direct_lvl` (no square-up yet)
- [ ] WebView shell with parameter relays/attachments for all parameters, plus a meter event channel (`m_in`, `m_out`)
- [ ] State save/restore via the APVTS XML

**Phase 2.1.2: Analysis Engine**
- [ ] Tracking HPF/LPF, ×4 decimator, YIN tracker (with confidence, octave guard, hold, note priority, speed/sens mapping)
- [ ] Onset/velocity detector and envelope follower
- [ ] Meters: `m_note`, `m_conf`, `m_env`
- [ ] Offline test harness: render the sample guitar DI files and log f0/confidence to CSV to check tracking accuracy

**Phase 2.1.3: Synth Source [C]**
- [ ] PolyBLEP oscillator (saw/square/pulse/tri/sine), osc octave/detune/mix, PWM
- [ ] Unison (1–7, spread and pan), sub osc, glide in log-frequency
- [ ] `src_synth_lvl` (temporarily gated by the follower until Phase 2.1.5)

**Phase 2.1.4: Octave Layer [B] & Square-Up**
- [ ] Rotating-head pitch shifter class (reused later by Slowdown)
- [ ] −12 / +12 / free shifters, plus `oct_*` and `src_oct_lvl`
- [ ] Square-Up waveshaper with ×2 oversampling on source A

**Phase 2.1.5: Synth Voice**
- [ ] Source mixer to stereo, then the ladder filter (type/cutoff/res/drive)
- [ ] Envelope modes Follow / ADSR / Swell, plus filter env amount and velocity sensitivity
- [ ] Rules for applying the envelope to A/B vs C (see architecture)

**Phase 2.1.6: FX Chain I**
- [ ] A bypass-crossfade wrapper for each FX block
- [ ] Slowdown (shifter + drift), Drive (4 modes, ×2 OS, tone), Ensemble (3 modes), Wobble/Age (wow, flutter, age, dropouts)

**Phase 2.1.7: FX Chain II**
- [ ] Haze: pre-delay, reverse-grain buffer, diffusers, 8-line modulated FDN, damping, RT60 mapping, freeze
- [ ] Echo: synced/free time, feedback loop with tone + limiter, BPM from the playhead

**Phase 2.1.8: Modulation & Macro**
- [ ] Control-rate tick (every 32 samples) with a destination offset table
- [ ] LFO 1/2 (shapes, free/sync, PPQ phase sync) and routing
- [ ] VAPOR macro offsets, plus width (M/S)

**Phase 2.1.9: Optimization & Polish**
- [ ] Real-time safety audit: no locks or allocations in `processBlock`, `ScopedNoDenormals`, a smoothed value for every continuous parameter
- [ ] CPU profiling. Target: < 8 % of one core at 48 kHz / 128 samples with everything on. Consider skipping inactive sources/FX entirely.
- [ ] Sample-rate checks (44.1 / 48 / 96 / 192 kHz) and block-size checks (1–2048, variable)
- [ ] Factory presets: Neon Lead, Mall at 3AM, Sunset Bass, Plastic Choir, Broken VHS
- [ ] pluginval run (strictness 5+) and handoff to the test phase

## Dependencies

**Required JUCE Modules:**
- `juce_audio_basics`
- `juce_audio_processors`
- `juce_dsp`: IIR, LadderFilter, Oversampling, DelayLine, SmoothedValue
- `juce_gui_basics`, `juce_gui_extra`: WebBrowserComponent / WebView2
- `juce_audio_utils`: Standalone wrapper

**Interop:** `@juce-framework/webview` (or `native/typescript/webview-interop/dist/index.js`) for the parameter relays.

**External:** none. YIN, PolyBLEP, FDN and the shifters are written in-house.

## Risk Assessment

**High Risk:**
- **Pitch tracking quality on guitar.** Risks are octave errors on the attack, unstable pitch on decay and harmonics, and garbage on chords. *Mitigations:* the octave guard, median filtering, hold, and a confidence-gated source C. The UI warns when confidence is low. Tune against real DI recordings in Phase 2.1.2 before building on top of it.
- **Tracking latency vs. low notes.** A 42 ms window is needed for the low E. *Mitigation:* the A/B sources cover the attack, and the oscillators fade in as confidence rises.
- **CPU budget.** Risk comes from 4 shifters, 2 oversampled stages, unison ×14, the FDN and the ensemble running together. *Mitigation:* skip processing for anything that is bypassed or at zero level, and update modulation at control rate.

**Medium Risk:**
- Grain artifacts in the pitch shifters (warble on sustained chords). Tune the grain length per use; a correlation-aligned splice is a possible upgrade.
- Feedback runaway when Freeze, high echo feedback and drive are combined. Needs a limiter in the echo loop, a gain-normalised FDN, and the final safety clip.
- Envelope/gate interactions (chattering on decay). Needs gate hysteresis and the retrigger lockout.
- Binding the WebView for 82 parameters plus a 30 Hz meter stream. Generate the relay list from one shared ID table; throttle meter events to visibility.
- Parameter smoothing for cutoff, delay time and pitch to avoid zipper noise and clicks.

**Low Risk:**
- Input/output gain, dry/wet, width, noise gate
- Ladder filter (it is in JUCE), ADSR, LFOs
- Bitcrush and the drive waveshapers
- APVTS state management
