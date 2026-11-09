# Creative Brief: RitualComp76

## Hook
**The FET legend, stripped to the bone.** RitualComp76 models the fast, aggressive, colored character of the classic 1176-style FET limiting amplifier — microsecond attack, program-dependent release, the infamous "all buttons in" smash — wrapped in a clean, modern, flat interface that gets out of your way.

## Description

### Sonic Character
- **Fast:** Attack from ~20 µs to ~800 µs. Catches transients no VCA or opto can. Fast enough to shape waveform cycles (low-frequency distortion at fastest settings — a feature, not a bug).
- **Aggressive:** Feedback-style FET gain reduction with soft, rounded knee that tightens as ratio rises. Pushes drums, vocals, bass forward in the mix.
- **Colored:** Class-A style output stage saturation and transformer coloration. Adds harmonic density (mostly 2nd/3rd order) even with little gain reduction. Input drive is part of the tone — hit it harder, get more grit.

### Behavior (faithful to original hardware)
- **No threshold control.** Threshold is fixed; the **Input** knob drives signal into the compressor. More input = more compression. **Output** sets makeup level.
- **Attack & Release knobs run "backwards":** 1 = slowest, 7 = fastest (fully clockwise = fastest), as on the original.
- **Release is program-dependent:** ~50 ms to ~1100 ms nominal, with recovery shaped by how hard the unit is driven (multi-stage release behavior).
- **Ratio buttons:** 4:1, 8:1, 12:1, 20:1, plus **All Buttons In** ("British mode"): ratio collapses toward ~12–20:1 with a pronounced knee change, attack/release timing shifts, heavy distortion, and the meter pins hard left — the signature room-mic/drum-smash sound.
- **All Buttons Out:** no ratio engaged — compressor out of circuit, signal passes through amplifier stages for pure color (classic "preamp only" trick).
- **VU Meter Modes:** GR (gain reduction), +4 and +8 (output level reference), Off.

### Use Cases
- Vocals: fast catch, presence, upfront aggression.
- Drums / room mics: All Buttons In parallel smash.
- Bass: consistent level, added grit.
- Guitars: sustain and bite.

### Visual Direction
- **Modern minimal flat look.** No skeuomorphic brushed metal or screws.
- Flat dark panel, single accent color, crisp typography.
- Large, legible Input/Output knobs; smaller Attack/Release.
- Ratio as a row of flat toggle buttons (multi-select allowed for "All").
- Flat, clean VU/GR meter (needle or bar — decided in Design phase) with smooth ballistics.

### Deliberately Out of Scope (v1)
To stay faithful to the original control set, v1 excludes: Mix/parallel knob, sidechain HPF, threshold, lookahead. Candidates for v2.
