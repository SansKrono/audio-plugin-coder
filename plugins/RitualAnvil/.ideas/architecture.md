# DSP Architecture Specification: RitualAnvil

Re-implementation of the Ritual Suite POC drum-bus chain as dedicated C++ classes (no JSON node graph). Stage order, fixed values and macro targets come from `parameter-spec.md`.

## Core Components

| Class (Source/) | Role | Built on |
| :--- | :--- | :--- |
| `ParameterIDs.hpp` | String IDs + APVTS layout (15 params) | `AudioProcessorValueTreeState` |
| `MacroMapper.h` | Pure function: manual values + `mix_ready` → effective values (additive law, below) | header-only, no state; unit-testable |
| `BandSplitter3.h` | Phase-coherent 3-band split (LR4 crossovers + allpass comp on low band) | `juce::dsp::LinkwitzRileyFilter` |
| `InputStage.h` | Trim gain + 12 dB/oct HPF (Q 0.707) | `SmoothedValue`, `dsp::IIR` / SVF |
| `TransientShaper.h` | Fast/slow envelope shaper (fast 1/30 ms, slow 20/200 ms), stereo-linked | port of POC `Transient_Shaper` |
| `ToneEQ.h` | 5 fixed peaking bands (60 / 200 / 3k / 8k / 12k Hz) | `dsp::IIR::Coefficients::makePeakFilter` |
| `PunchStage.h` | Parallel comp (−20 dB, 8:1, 3/150 ms) + wet blend 0–0.5 | `dsp::Compressor`, dry copy prealloc |
| `HeatStage.h` | 4× oversample → 3-band split @ 200 Hz / 5 kHz → hard / tanh / asym tube → sum → downsample | `dsp::Oversampling` (FIR halfband, linear phase), `BandSplitter3` |
| `WidthStage.h` | M/S: side × width, side HPF 200 Hz | 1-pole/2-pole HPF on S |
| `TruePeakLimiter.h` | Lookahead (~1.5 ms) brickwall, 4× OS peak detection, release 150 ms | delay line + gain smoother |
| `AnvilProcessor` (`PluginProcessor`) | Owns stages, reads params once per block, runs `MacroMapper`, pushes targets, metering atomics, latency, bypass crossfade | `AudioProcessor` |
| `PluginEditor` | WebView host + relays + meter event emitter | `WebBrowserComponent`, `WebSliderRelay`, `WebToggleButtonRelay` |

## Processing Chain

```
In ─► InputStage (trim → HPF)
   ─► BandSplit 300 Hz / 20 kHz ─┬─ Low  ─► TransientShaper ─┐
                                 ├─ Mid  ─► TransientShaper ─┼─► Σ
                                 └─ High ─► (dry) ───────────┘
   ─► ToneEQ (Kick → Box → Snare → Presence → Air)
   ─► PunchStage (dry + comp·blend)
   ─► HeatStage [↑4× ─ split 200 / 5k ─ hard | soft | tube ─ Σ ─ ↓4×]
   ─► WidthStage (M/S, side HPF 200 Hz)
   ─► TruePeakLimiter (ceiling)
   ─► Out
Bypass: latency-matched dry delay, 10 ms crossfade.
```

Notes:
- Transient shapers share one stereo-linked detector per band (max of |L|,|R|) to avoid image shift.
- At neutral settings (all 0 dB, punch 0, heat 0, width 100) the chain is magnitude-flat (LR sums) — tested.
- Heat bypasses the OS path entirely when effective drive = 0 *and* smoother settled, but latency stays constant (OS always runs; skip only nonlinear math) so no latency jumps.

## Parameter Mapping

| Parameter | Component | Function | Range |
| :--- | :--- | :--- | :--- |
| `mix_ready` | MacroMapper | Offsets for HPF, EQ, transients, punch, heat, width | 0–100 % |
| `input_gain` | InputStage | Trim gain, smoothed 20 ms | ±12 dB |
| `hpf` | InputStage | HPF cutoff, smoothed 50 ms, log | 20–100 Hz |
| `transient_attack` | TransientShaper ×2 | Transient gain | −12…+12 dB |
| `transient_sustain` | TransientShaper ×2 | Body gain | −12…+6 dB |
| `eq_kick` … `eq_air` | ToneEQ | Band gain, coeffs per 32-sample sub-block | ±12 dB |
| `punch` | PunchStage | Wet blend = punch/100 × 0.5 | 0–100 % |
| `heat` | HeatStage | Drive into all 3 shapers, auto output-comp (−drive/2 dB approx, tuned) | 0–20 dB |
| `width` | WidthStage | Side gain = width/100 | 50–150 % |
| `ceiling` | TruePeakLimiter | Ceiling | −1.0…−0.1 dBTP |
| `bypass` | Processor | Crossfaded bypass (`getBypassParameter`) | bool |

### Mix Ready — Additive Law (decided)
`t = mix_ready / 100`. Effective value = `clamp(manual + offset(t), paramMin, paramMax)`. Manual knobs always live.

| Target | Offset(t) | Notes |
| :--- | :--- | :--- |
| HPF | `max(manual, 20 + 30t)` | floor, not sum — avoids runaway HPF. Floor starts at param min (20 Hz) so no jump at t = 0⁺ (POC jumped to 30 Hz); same 50 Hz at full |
| Kick / Box / Snare / Pres / Air | `+4t / −3t / +3t / +2t / +4t` dB | |
| Transient attack | `t > 0.5 ? (t−0.5)·8 : 0` dB | |
| Transient sustain | `t < 0.5 ? −3·(1 − |t−0.25|/0.25) : 0` dB | triangle dip, −3 dB at 25 %, 0 at 0 and 50 %. POC jumped straight to −3 dB at t = 0⁺; made continuous |
| Punch blend | `+clamp((t−0.4)·0.5/0.6, 0, 0.5)` | total blend clamped to 0.5 |
| Heat drive | `t > 0.6 ? (t−0.6)·15 : 0` dB | total clamped to 20 dB |
| Width | `+clamp((t−0.5)·2, 0, 1)·0.2` (×100 %) | total clamped 50–150 % |

Effective values exposed to UI (atomics) so knobs can show a "ghost" ring of macro contribution.

## Real-Time Rules
- No allocation in `processBlock`; all buffers sized in `prepareToPlay` (max block × 4 for OS).
- `ScopedNoDenormals`; params read once per block; gains per-sample smoothed; filter coeffs updated per 32-sample sub-block while smoothing.
- `setLatencySamples(heatOS.getLatencyInSamples() + limiterLookahead)` — recomputed on sample-rate change.

## Metering (to UI, ~30 Hz timer)
Input peak, output true-peak, limiter GR, Punch comp GR, effective macro values. Atomics written on audio thread, read on message thread.

## Complexity Assessment
**Score: 3 (Advanced)**
**Rationale:** Multiband processing in two places (with phase-coherent recombination), oversampled nonlinear stage, lookahead true-peak limiter with latency reporting, and a macro layer mapping one control onto 11 targets. No synthesis, analysis, or modeling research — each stage is a known algorithm — so not 4.
