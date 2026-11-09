# Creative Brief: RitualAnvil

## Hook
**One knob from raw kit to mix-ready drums.** RitualAnvil is a complete drum-bus finishing chain — cleanup, transient shaping, tone EQ, parallel punch, multiband heat, width and a safety limiter — forged into one strip. Turn **Mix Ready** and the whole chain moves together; open the hood when you want to dial each stage yourself.

## Origin
Re-implementation of **Ritual Anvil** from the Ritual Suite POC (`ritual-suite/RitualAnvil`). The POC built Anvil from a JSON-driven generic node graph (`anvil_config.json` + `RitualEngine`) with a generic JUCE slider UI. This version keeps the POC's signal chain, control set and macro behavior, but rebuilds it as a dedicated, standalone APC plugin (own DSP classes, purpose-built UI, no runtime graph loader).

## Description

### Signal Chain (faithful to POC order)
1. **Input** — trim gain (±12 dB) → 12 dB/oct high-pass (20–100 Hz) to clear sub rumble.
2. **Transient** — 3-band split (300 Hz / 20 kHz). Low and mid bands get a fast/slow envelope transient shaper (Attack / Sustain in dB). Highs pass untouched to avoid cymbal splatter.
3. **Tone EQ** — five fixed-frequency peaking bands voiced for drums:
   - Kick 60 Hz · Box 200 Hz (cut target) · Snare 3 kHz · Presence 8 kHz · Air 12 kHz.
4. **Punch** — parallel compression (−20 dB thresh, 8:1, 3 ms / 150 ms) blended up to 50% wet.
5. **Heat** — 3-band saturation (200 Hz / 5 kHz): lows = hard clip, mids = soft clip (tanh), highs = asymmetric tube. One Drive control across all bands.
6. **Width** — M/S width (50–150%) with side high-pass at 200 Hz to keep kick/bass mono.
7. **Ceiling** — true-peak limiter, −1.0 to −0.1 dBTP.

### Mix Ready Macro (POC behavior, 0–100%)
As the macro rises it pushes the chain toward a "finished" drum bus:
- HPF 30 → 50 Hz
- EQ: Kick +4, Box −3, Snare +3, Presence +2, Air +4 dB (linear with macro)
- Transients: sustain trimmed below 50%, attack boosted above 50% (up to +4 dB)
- Punch blend engages above 40%, reaching 50% at full
- Heat engages above 60%, reaching 6 dB drive at full
- Width 100 → 120% above 50%

**Re-implementation change:** in the POC, any macro value > 0 *overwrote* the manual controls (manual knobs went dead). Here the macro becomes an **offset layered on top of manual settings**, so users can set a base sound and then ride Mix Ready. Exact blend law decided in `/plan`.

### Fixes Over the POC
- Oversampling on the Heat stage (POC hard-clipped with no anti-aliasing).
- Parameter smoothing on all gain/frequency moves (POC set node values per-block, zipper-prone).
- Proper latency reporting for limiter lookahead / oversampling.
- Phase-coherent band splits (Linkwitz-Riley) so Transient and Heat splits sum flat at neutral settings.
- Real true-peak detection on the limiter.

### Use Cases
- Drum bus / drum stem finishing in one insert.
- Quick "make it sound like a record" on programmed or sampled kits.
- Room/overhead bus with Heat + Punch.

### Visual Direction
Ritual Suite family look (POC palette: background `#141619`, warm bone labels `#F2E6D8`).
- **Hero:** large central **Mix Ready** dial — the forge / anvil focal point.
- Surrounding stage modules laid out left→right in signal order: Input · Transient · EQ · Punch · Heat · Width · Ceiling.
- Dark forged-metal feel, ember/orange accent that glows hotter as Heat and Mix Ready rise.
- Output meter with limiter gain-reduction readout.
- Final framework (WebView vs Visage) chosen in `/plan`; sibling RitualComp76 uses WebView.

### Out of Scope (v1)
Presets browser beyond host presets, sidechain input, adjustable EQ frequencies/Q, per-band Heat drive, dry/wet for the whole chain. Candidates for v2.
