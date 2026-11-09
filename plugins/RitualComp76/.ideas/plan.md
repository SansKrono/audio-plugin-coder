# Implementation Plan: RitualComp76

## Complexity Score: 3

## Implementation Strategy: Phased

### Phase 2.1.1: Core Processing
- [x] APVTS with all 7 params (`input`, `output`, `attack`, `release`, `ratio`, `meter_mode`, `bypass`), IDs matching parameter-spec.md
- [x] Input/Output attenuators with log taper + ∞ (mute) at bottom, `SmoothedValue`
- [x] Feedback peak detector, linked stereo
- [x] Attack/release one-pole ballistics with exp time mapping (1–7 → µs/ms)
- [x] Gain computer: fixed threshold, 4/8/12/20 ratios, ratio-dependent soft knee
- [x] `Out` mode (GR disabled, amp stages active)
- [x] GR + VU meter atomics exposed to editor

### Phase 2.1.2: Character & Optimization
- [x] 4× `juce::dsp::Oversampling` around nonlinear stages; report latency via `setLatencySamples`
- [x] Input transformer saturation + LF/HF shaping
- [x] FET cell GR-dependent asymmetric distortion
- [x] Class-A output stage soft clipper
- [x] Program-dependent two-stage release
- [x] `All` (all-buttons-in) mode: variable ratio, knee overshoot, timing shift, extra distortion, meter pin
- [x] Real-time safety: no allocations in `processBlock`, coefficient recompute per block, denormal guard (`ScopedNoDenormals`)
- [x] CPU check: target < 3% single instance @ 48 kHz, 4× OS (measured 1.2%)

### Phase 2.1.3: Polish
- [x] Click-free ratio switching (≈10 ms crossfade), bypass crossfade with latency-matched dry path
- [x] Edge cases: sample-rate change, block size 1, silence/denormals, -∞ input/output
- [x] State save/restore (APVTS XML), preset defaults
- [ ] Calibration pass: objective gain-staging/GR checks done; by-ear tuning of GR curve, release feel, All-mode still pending (needs listening)
- [x] pluginval / test integration

## Dependencies

**Required JUCE Modules:**
- juce_audio_basics
- juce_audio_processors
- juce_dsp (Oversampling, SmoothedValue, filters)
- juce_gui_extra (WebBrowserComponent)

**Platform WebView:**
- macOS: WKWebView (system), neither `NEEDS_WEBVIEW2` nor `NEEDS_WEB_BROWSER`
- Windows: `NEEDS_WEBVIEW2 TRUE`
- Linux: `NEEDS_WEB_BROWSER TRUE` (webkit2gtk)

**External:** none.

## UI Framework

**Decision:** WebView
**Rationale:** User selected it. A flat, minimal canvas UI iterates quickly with browser preview, and it matches the drumBussProcessor workflow. The GR/VU meter needs smooth ballistics, which the canvas handles easily via a ~30–60 Hz timer pushing meter values over the JUCE bridge.

## Risk Assessment

**High Risk:**
- Feedback detector stability at 20 µs attack. Needs the oversampled rate, and coefficients must stay inside stable bounds.
- All-mode behavior is subjective, so expect several tuning iterations.
- Aliasing from stacked nonlinear stages. Verify with sweeps and adjust the OS factor or filter type.

**Medium Risk:**
- Program-dependent release feeling natural (blend curve tuning).
- Latency reporting plus a bypass that stays phase-aligned.
- WebView ↔ APVTS binding for the Choice param shown as four toggle buttons (multi-press → `All`, none → `Out`).
- Meter update throughput over the WebView bridge.

**Low Risk:**
- Attenuator gain stages and smoothing.
- Parameter layout and state save/restore.
- Meter-mode switching (display only).
