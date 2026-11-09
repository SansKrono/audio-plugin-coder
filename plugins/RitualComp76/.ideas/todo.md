# RitualComp76 — TODO

## Phase 1: DREAM
- [x] Creative brief
- [x] Parameter spec

## Phase 2: PLAN
- [x] DSP architecture (feedback FET, 4× OS, two-stage release)
- [x] Implementation plan (complexity 3, phased)
- [x] UI framework: WebView

## Phase 3: DESIGN
- [x] v1 UI spec (760×380, horizontal strip)
- [x] v1 style guide (Amber Signal)
- [x] v1-test.html preview (canvas knobs, needle VU, ratio/All logic)
- [x] Design approved (v1)

## Phase 4: CODE (/impl)
- [x] 2.1.1 Core processing (APVTS, attenuators, feedback detector, ballistics, gain computer, Out mode, meter atomics)
- [x] 2.1.2 Character (4× OS, transformer, FET distortion, class-A stage, program-dependent release, All mode)
- [ ] 2.1.3 Polish (click-free switching, bypass crossfade, state, calibration, pluginval)
- [x] Port v1-test.html → WebUI, wire JUCE relays (sliders, ratio combo, meter_mode combo, bypass toggle)
- [x] Native → JS meter event (GR + VU level, ~30–60 Hz)

## Phase 5: SHIP
- [ ] Build, test, package
