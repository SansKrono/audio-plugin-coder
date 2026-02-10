# DSP Architecture Specification: drumBussProcessor

## Core Components
The plugin implements a complex mastering-grade chain for drum busses, controlled by a single macro.

1.  **Input Gain:** Digital trim (-24 to +24 dB).
2.  **Input Clipper:** Hard safety clipper to shave transients before compression.
3.  **VCA Compressor (Glue):**
    *   Style: SSL G-Bus / FG-Grey.
    *   Characteristics: 4:1 Ratio, 30ms Attack, 100ms Release (auto).
    *   Role: Cohesion and punch.
4.  **Transformer Distortion (Brit):**
    *   Style: Neve-style saturation.
    *   Role: Harmonics and grit.
5.  **FET Compressor (Parallel):**
    *   Style: 1176 "All Buttons In" / Monster.
    *   Configuration: Fixed aggressive settings, mixed in parallel.
    *   Role: Sustain and body.
6.  **Exciters:**
    *   **Low (Earth):** Sub-harmonic synthesis or resonant high-pass.
    *   **High (Shimmer):** High-shelf harmonic generation.
7.  **Equalizer:**
    *   Style: Pultec broad strokes.
    *   Bands: Low boost (60Hz), High boost (10kHz).
8.  **Output Clipper:** Hard clipper for final loudness.
9.  **Output Gain:** Digital trim (-24 to +24 dB).

## Processing Chain
```
Input 
  → Input Trim 
  → Input Clipper 
  → VCA Compressor (Glue) 
  → Distortion (Brit) 
  → [Split] -> FET Compressor (Monster) -> [Merge (Parallel Mix)]
  → Exciters (Earth & Shimmer) 
  → EQ (Pultec Style) 
  → Output Clipper 
  → Output Trim 
  → Output
```

## Parameter Mapping
The `mix_ready` knob is a macro that orchestrates the entire chain.

| Parameter | Component | Function | Range | mapping |
| :--- | :--- | :--- | :--- | :--- |
| `input_trim` | Input Gain | Signal level | +/- 24dB | 1:1 |
| `output_trim` | Output Gain | Signal level | +/- 24dB | 1:1 |
| `mix_ready` | VCA Comp | Threshold | 0 to -20dB | Inverse linear (0% = 0dB, 100% = -20dB) |
| `mix_ready` | VCA Comp | Makeup Gain | 0 to 6dB | Linear |
| `mix_ready` | Distortion | Drive | 0% to 50% | Linear |
| `mix_ready` | FET Comp | Wet Mix | 0% to 40% | Logarithmic |
| `mix_ready` | Exciters | Amount | 0% to 50% | Linear |
| `mix_ready` | EQ | Low/High Boost | 0dB to 4dB | Linear |

## Complexity Assessment
**Score: 3 (Advanced)**
*   **Rationale:** The individual components (Comp, Dist, EQ) are standard, but the **orchestration** and careful gain staging required for the single-knob macro to sound good "mix-ready" at all positions adds significant tuning complexity. The parallel path also adds architectural detail.
