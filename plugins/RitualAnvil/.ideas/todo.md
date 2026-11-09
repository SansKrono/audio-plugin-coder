# RitualAnvil — TODO

## Phase 1: DREAM
- [x] Creative brief (POC re-implementation)
- [x] Parameter spec (15 params, POC ranges)

## Phase 2: PLAN
- [x] DSP architecture (7-stage chain, LR4 splits, 4× OS Heat, TP limiter, MacroMapper)
- [x] Implementation plan (complexity 3, phased)
- [x] UI framework: WebView
- [x] Macro law: additive offsets (HPF floor 20+30t, continuous sustain dip)

## Phase 3: DESIGN
- [x] v1 UI spec (1000×600, hero Mix Ready + stage cards)
- [x] v1 style guide (Amber Signal + ember macro accent)
- [x] v1-test.html preview (ghost arcs, EQ curve, IN/OUT + GR meters)
- [x] Design approved (v1)

- [x] v2 redesign with GlassKit (forge aurora, glass knobs, 1100×660), approved

## Phase 4: CODE (/impl)
- [x] 4.1.1 Core signal path (60 offline checks pass)
- [x] 4.1.2 Quality & optimization (OS Heat, TP limiter, latency 157 smp @ 48 k, 0 allocs, CPU ~3.3 %)
- [x] 4.1.3 Polish (edge cases, click-free jumps, heat compensation calibration, pluginval/auval; 100 checks pass)
- [x] Mix Ready 25 % loudness dip removed (sustain no longer macro-driven)
- [ ] By-ear voicing sign-off: Mix Ready curve + Heat character, renders in build/RitualAnvil_renders
- [x] 4.2 Port v1-test.html → WebUI, relays, meter + effective-value events (done with 4.1.1)
- [x] v2 GlassKit UI ported to Source/ui/public; checked in the Standalone (WKWebView) + pluginval editor tests
- [ ] Open the editor in a DAW (CPU/GPU of ~10 backdrop-filter layers; fallback data-surface="solid")
- [ ] macOS: brief white flash while the WebView loads

## Phase 5: SHIP
- [x] macOS beta package v0.3.0-beta: universal VST3/AU/Standalone, ad-hoc signed, pkg + dmg + zip (dist/)
- [ ] Developer ID signing + notarization
- [ ] Windows / Linux builds (fix CI tag trigger first)
- [ ] Review EULA (APC template) before public release
