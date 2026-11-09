# Implementation Plan: RitualAnvil

## Complexity Score: 3

## Implementation Strategy: Phased

### Phase 4.1.1: Core Signal Path
- [x] `ParameterIDs.hpp` + APVTS layout: 15 params, IDs/ranges per parameter-spec.md, log skew on `hpf` (centre 44.7 Hz)
- [x] `MacroMapper.h` (pure, additive law) + unit tests for every target at t = 0, 0.01, 0.25, 0.4, 0.5, 0.6, 0.75, 1.0 and clamp edges
- [x] `InputStage`, `ToneEQ`, `WidthStage` with smoothing (own RBJ biquads, no allocation; coeffs every 32 samples on a block-independent counter)
- [x] `BandSplitter3` (LR4 + allpass comp) — flat-sum test (< 0.01 dB)
- [x] `TransientStage` (POC shaper port, stereo-linked detector per band) on low + mid bands
- [x] `PunchStage` (own linked peak detector instead of dsp::Compressor so GR can be metered; dry·(1−b) + wet·b like POC)
- [x] Heat **without** oversampling (hard / tanh / tube per band, fade-in over first 3 dB, −drive/2 output comp, 5 Hz DC blocker)
- [x] Simple peak limiter placeholder (instant attack, 150 ms release); meter + effective-value atomics
- [x] CMake target (VST3 / AU / Standalone on macOS; WebView flags per platform, RitualComp76 pattern) + `RitualAnvil_DspTest`
- [x] WebView editor: 14 slider relays + bypass toggle (relays → WebView → attachments), embedded zip, `meter` event with `eff`
- [x] Bypass crossfade (10 ms); state save/restore

### Phase 4.1.2: Quality & Optimization
- [x] Heat: 4× `dsp::Oversampling` (FIR half-band equiripple, integer latency), split + shapers at OS rate; OS path always runs (constant latency/phase). Aliases of 9 kHz @ 20 dB drive ≤ −68 dBc. Output compensation tuning → 4.1.3 (by ear)
- [x] `TruePeakLimiter`: 1.5 ms lookahead, 4× windowed-sinc detection (32 taps/phase) + parabolic refinement, 150 ms release, min-hold ±8 samples around detected peaks + moving-average attack. True peak ≤ ceiling + 0.04 dB (sines up to 19 kHz, classic fs/4 ISP, noise bursts at mix 100 %)
- [x] Latency reporting = OS (61 smp) + lookahead (72 @ 48 k) + interp (16) + guard (8) = 157 @ 48 kHz; reported from construction; constant over params; dry/bypass path delayed to match
- [x] Coefficient sub-block updates, `ScopedNoDenormals`, zero heap allocations in `processBlock` (verified with an operator-new counter in the test)
- [x] CPU 3.2–3.5 % of real time (process CPU time), single instance @ 48 kHz, everything engaged (target < 5 %). Heat gains cached while Drive is steady; Punch skips log/exp below threshold

### Phase 4.1.3: Polish
- [ ] Bypass crossfade with latency-matched dry path
- [ ] Edge cases: SR change, block size 1, mono in, silence, max-everything (all +12 dB, heat 20, macro 100)
- [ ] State save/restore (APVTS XML), version tag
- [ ] Voicing pass: Mix Ready sweep on reference drum loops; adjust offsets only with sign-off
- [ ] pluginval (strictness 10) + auval

### Phase 4.2: UI Binding (after Design phase)
- [ ] WebView relays for all params, meter + effective-value events at 30 Hz
- [ ] Macro ghost rings driven by effective values

## Dependencies

**Required JUCE Modules:**
- juce_audio_basics
- juce_audio_processors
- juce_audio_utils
- juce_dsp (Oversampling, LinkwitzRileyFilter, IIR, Compressor, SmoothedValue)
- juce_gui_extra (WebBrowserComponent)

**Platform WebView:**
- macOS: WKWebView (system) — neither flag
- Windows: `NEEDS_WEBVIEW2 TRUE`
- Linux: `NEEDS_WEB_BROWSER TRUE` (webkit2gtk)

**External:** none. POC `RitualEngine` not linked — algorithms ported, not reused.

## UI Framework

**Decision:** WebView
**Rationale:** User selected. Matches sibling RitualComp76 / drumBussProcessor workflow (browser-preview iteration, shared relay glue); hero Mix Ready dial, glowing Heat accent, ghost macro rings and meters are easy on Canvas.

## Risk Assessment

**High Risk:**
- True-peak limiter accuracy vs CPU (OS detection + lookahead).
- Heat aliasing / tone — hard clip on lows is harsh; may need softened knee after listening.
- Two cascaded 3-band splits: phase/flatness must hold at neutral; group delay smears transients if splits mis-designed.

**Medium Risk:**
- Additive macro + manual can stack to extremes (e.g. Air +12 manual +4 macro → clamped); voicing must sound good across the space.
- Filter coefficient smoothing during fast automation (zipper / instability on HPF sweeps).
- Bypass + latency compensation correctness across hosts.

**Low Risk:**
- Input trim, Width M/S, parameter layout, state, metering atomics.
