#pragma once
/*  VaporStrings — modulation (phase 2.1.8). Dependency-free (std only).

    Lfo          sine / triangle / saw / square / S&H, bipolar −1…+1.
                 Free: rate in Hz. Synced: the rate knob picks a note value —
                 cycles per beat = 2^round(log2(rate)) (1 Hz → 1/4, 2 → 1/8, 0.5 → 1/2 …),
                 phase locked to the host PPQ while the transport runs.
    Destinations (per LFO, scaled by its amount): Cutoff ±4 oct, Pitch ±1 st,
                 PWM ±0.4, Ens Depth ±0.5, Haze Size ±0.3, Volume (tremolo).
    VaporMacro   one knob, offsets applied on top of the user's parameters (the
                 parameters themselves are never written, so automation stays clean).  */

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vs
{
class Lfo
{
public:
    enum class Shape { Sine = 0, Triangle, Saw, Square, SampleHold };

    /** Cycles per beat used when synced (see header). */
    static double syncedCyclesPerBeat (float rateKnobHz) noexcept
    {
        const double c = std::exp2 (std::round (std::log2 (std::clamp ((double) rateKnobHz, 1.0 / 16.0, 8.0))));
        return std::clamp (c, 1.0 / 16.0, 8.0);
    }

    void prepare (double sampleRate) { sr = sampleRate; phase = 0.0; held = 0.0f; }
    void setShape (Shape s) noexcept { shape = s; }

    /** Call once per block. ppq < 0 = transport stopped / unknown. */
    void beginBlock (float rateHz, bool sync, double bpm, double ppq) noexcept
    {
        if (sync)
        {
            const double c = syncedCyclesPerBeat (rateHz);
            inc = c * (std::clamp (bpm, 20.0, 400.0) / 60.0) / sr;
            if (ppq >= 0.0)
            {
                const double p = ppq * c;
                setPhase (p - std::floor (p));
            }
        }
        else
        {
            inc = std::clamp ((double) rateHz, 0.001, 50.0) / sr;
        }
    }

    /** Current value (−1…+1). */
    float value() const noexcept
    {
        const double p = phase;
        switch (shape)
        {
            case Shape::Sine:       return (float) std::sin (6.283185307179586 * p);
            case Shape::Triangle:   return (float) (1.0 - 4.0 * std::abs (p - 0.5)) ;
            case Shape::Saw:        return (float) (2.0 * p - 1.0);
            case Shape::Square:     return p < 0.5 ? 1.0f : -1.0f;
            case Shape::SampleHold: return held;
        }
        return 0.0f;
    }

    /** Advance by n samples (control-rate tick). */
    void advance (int n) noexcept
    {
        phase += inc * n;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            newHold();
        }
    }

    double getPhase() const noexcept { return phase; }

private:
    void setPhase (double p) noexcept
    {
        if (p < phase && phase - p > 0.5) newHold();     // wrapped since last block
        phase = p;
    }
    void newHold() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        held = (float) (rng >> 8) / 8388608.0f - 1.0f;
    }

    double sr = 48000.0, phase = 0.0, inc = 0.0;
    Shape shape = Shape::Sine;
    float held = 0.0f;
    uint32_t rng = 0xC0FFEEu;
};

//==============================================================================
enum class ModDest { Cutoff = 0, Pitch, PWM, EnsDepth, HazeSize, Volume, Count };

struct ModValues
{
    float cutoffOct = 0, pitchSt = 0, pwm = 0, ensDepth = 0, hazeSize = 0, volume = 1;

    void clear() noexcept { *this = ModValues{}; }

    /** Add one LFO's contribution (lfo −1…+1, amount −1…+1). */
    void add (ModDest d, float lfo, float amount) noexcept
    {
        const float v = lfo * amount;
        switch (d)
        {
            case ModDest::Cutoff:   cutoffOct += 4.0f * v; break;
            case ModDest::Pitch:    pitchSt   += 1.0f * v; break;
            case ModDest::PWM:      pwm       += 0.4f * v; break;
            case ModDest::EnsDepth: ensDepth  += 0.5f * v; break;
            case ModDest::HazeSize: hazeSize  += 0.3f * v; break;
            case ModDest::Volume:
            {
                // tremolo: dips toward silence by |amount| (sign flips the phase)
                const float a = std::abs (amount);
                const float s = amount >= 0.0f ? lfo : -lfo;
                volume *= 1.0f - a * 0.5f * (1.0f - s);
                break;
            }
            case ModDest::Count: break;
        }
    }
};

//==============================================================================
/** VAPOR macro: v 0…1 → offsets (see .ideas/parameter-spec.md). */
struct VaporMacro
{
    float hazeMix = 0, wobWow = 0, wobAge = 0, slowPitchSt = 0, attackMul = 1, ensDepth = 0;
    float synthGain = 1, directGain = 1, swellBlend = 0;
    bool engageHaze = false, engageWobble = false;

    static VaporMacro from (float v) noexcept
    {
        v = std::clamp (v, 0.0f, 1.0f);
        VaporMacro m;
        m.hazeMix     = 0.5f * v;
        m.wobWow      = 0.5f * v;
        m.wobAge      = 0.5f * v;
        m.slowPitchSt = -5.0f * v;          // applied only while Slowdown is on
        m.attackMul   = 1.0f + 39.0f * v;   // ×1 … ×40
        m.ensDepth    = 0.4f * v;
        m.synthGain   = 1.0f + 1.0f * v;    // +6 dB toward the synth …
        m.directGain  = 1.0f - 0.5f * v;    // … −6 dB from the direct guitar
        m.swellBlend  = v;                  // Follow mode blooms in with VAPOR
        // the macro owns the "dissolve" FX: it engages Haze and Wobble once turned up
        m.engageHaze   = v > 0.02f;
        m.engageWobble = v > 0.02f;
        return m;
    }
};

} // namespace vs
