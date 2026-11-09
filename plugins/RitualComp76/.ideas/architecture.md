# DSP Architecture Specification: RitualComp76

1176-style FET limiting amplifier. Feedback topology, fixed threshold, input-driven compression, colored amplifier stages.

## Core Components

1. **Input Attenuator**: smoothed linear gain from `input` (log taper, -∞ to 0 dB atten, plus fixed internal gain so 0 dB atten ≈ +45 dB of drive into the cell).
2. **Input Transformer Stage**: gentle low-frequency saturation (tanh-style, level dependent), slight HF roll-off (~20 kHz one-pole) and LF coupling (~10 Hz HPF).
3. **FET Gain Cell**: variable gain element applying the gain reduction. Adds GR-dependent asymmetric distortion (2nd harmonic dominant) that scales with how much reduction is happening, which is the FET channel nonlinearity.
4. **Sidechain Detector (feedback)**: taps the **output of the gain cell** (feedback design, not feed-forward). Full-wave peak rectifier.
5. **Ballistics**:
   - Attack: one-pole, 20 µs to 800 µs (exp map from `attack` 7 to 1).
   - Release: **program-dependent two-stage** release, with a fast stage (50 to 1100 ms from `release`) and a slow stage (≈3 to 5× the fast time). Blend weight rises with sustained GR, so recovery slows after long or heavy compression.
6. **Gain Computer**: fixed threshold, ratio from `ratio`, soft knee whose width narrows as ratio rises (4:1 is widest, 20:1 is nearly hard).
   - **All mode**: effective ratio is not constant (≈12:1 near threshold rising past 20:1). Knee overshoot ("bump") pumps GR slightly past the target. Attack and release times scale (attack slower, release faster). Distortion gain in the FET stage is raised. Detector bias adds a static GR offset so the meter pins.
   - **Out mode**: gain computer disabled (GR = 0 dB). Amplifier stages stay active for color only.
7. **Output Amplifier (Class-A style)**: asymmetric soft clipper with mild even/odd harmonics. Drive depends on level after makeup.
8. **Output Attenuator**: smoothed gain from `output`.
9. **Oversampling**: `juce::dsp::Oversampling` 4× (polyphase IIR) wrapping stages 2 to 7 (all nonlinear stages and the fast detector, since 20 µs attack needs sample-rate headroom).
10. **Metering**: per-block GR (dB, peak-hold) and output level with VU ballistics (300 ms integration), sent to the UI through `std::atomic<float>`. `meter_mode` picks the source (+8 and +4 shift the VU reference).
11. **Bypass**: latency-compensated dry path (oversampler latency reported to host), with a short crossfade on toggle.

## Processing Chain

```
Input ─▶ Input Atten ─▶ ┌──────────── 4× Oversampled ────────────────────────────────┐ ─▶ Output Atten ─▶ Output
                        │ Input Xfmr ─▶ FET Gain Cell ─┬─▶ Class-A Out Amp            │
                        │                  ▲           │                              │
                        │                  │      Detector (peak, feedback tap)       │
                        │                  │           ▼                              │
                        │            Gain Computer ◀─ Ballistics (attack / 2-stage rel)│
                        └─────────────────────────────────────────────────────────────┘
                                                    │
                                              GR / VU meters ─▶ UI (atomics)
```

Stereo: two channels, **linked detector** (max of both channels' rectified levels), so one GR signal drives both cells and the stereo image stays put. Per-channel nonlinearities.

Feedback loop: the detector reads the previous sample's cell output (one-sample delay at 4× rate is negligible). This gives the program-dependent, "grabby" feedback character.

## Parameter Mapping

| Parameter | Component | Function | Range |
|-----------|-----------|----------|-------|
| `input` | Input Atten | Drive into fixed threshold. Sets amount of GR and saturation | -∞, -48 to 0 dB |
| `output` | Output Atten | Makeup / final level | -∞, -48 to 0 dB |
| `attack` | Ballistics | Attack coefficient (exp map) | 800 µs (1) to 20 µs (7) |
| `release` | Ballistics | Fast-stage release, slow stage scaled from it | 1100 ms (1) to 50 ms (7) |
| `ratio` | Gain Computer + FET Cell | Ratio, knee width, All-mode behavior, Out = no GR | Out / 4 / 8 / 12 / 20 / All |
| `meter_mode` | Metering | Meter source and reference, no audio effect | GR / +8 / +4 / Off |
| `bypass` | Bypass | Crossfaded hard bypass | Off / On |

Smoothing: `input` and `output` use `SmoothedValue` (multiplicative, 20 ms). `attack` and `release` recompute their coefficients once per block. A `ratio` change crossfades the gain-computer target over about 10 ms to avoid clicks.

## Complexity Assessment

**Score: 3 (Advanced)**

**Rationale:**
- More than a textbook compressor: the feedback detector, program-dependent two-stage release, ratio-dependent knee and the nonlinear All mode all need tuning by ear.
- Several nonlinear stages need oversampling, latency reporting and aliasing control.
- Microsecond attack times make the loop sensitive to numerical stability.
- Not level 4 or 5: no circuit-level (WDF/SPICE) modeling and no ML. This is behavioral modeling with standard JUCE DSP blocks.
