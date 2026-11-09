#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

/**
    Colour stages for RitualComp76 (Phase 4.1.2). All run at the oversampled rate.
    Each keeps state for up to kMaxChannels channels and is allocation-free.

    - InputTransformer: LF coupling (~10 Hz HPF), level-dependent saturation that
      bites harder on lows (iron core), gentle HF roll-off.
    - ClassAStage:      biased, asymmetric soft clipper (mixed even/odd harmonics),
                        unity small-signal gain.
    - DcBlocker:        removes DC introduced by asymmetric stages.
*/
namespace rc76
{
    constexpr int kMaxChannels = 2;

    inline float onePoleLowpassCoeff (float hz, double fs) noexcept
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) fs);
    }

    //==============================================================================
    class DcBlocker
    {
    public:
        void prepare (double fs, float cutoffHz = 8.0f) noexcept
        {
            r = std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) fs);
            reset();
        }
        void reset() noexcept { x1.fill (0.0f); y1.fill (0.0f); }

        float process (int ch, float x) noexcept
        {
            const auto y = x - x1[(size_t) ch] + r * y1[(size_t) ch];
            x1[(size_t) ch] = x;
            y1[(size_t) ch] = y;
            return y;
        }

    private:
        float r = 0.999f;
        std::array<float, kMaxChannels> x1 {}, y1 {};
    };

    //==============================================================================
    class InputTransformer
    {
    public:
        void prepare (double fs) noexcept
        {
            hp.prepare (fs, 10.0f);
            lfSplit = onePoleLowpassCoeff (150.0f, fs);
            hfRoll  = onePoleLowpassCoeff (24000.0f, fs);
            reset();
        }
        void reset() noexcept { hp.reset(); lf.fill (0.0f); out.fill (0.0f); }

        float process (int ch, float x) noexcept
        {
            const auto c = (size_t) ch;
            x = hp.process (ch, x);

            // Split: lows saturate harder than highs (core saturation is frequency dependent)
            lf[c] += lfSplit * (x - lf[c]);
            const auto lows  = lf[c];
            const auto highs = x - lows;
            const auto shaped = sat (lows, lowDrive) + sat (highs, highDrive);

            out[c] += hfRoll * (shaped - out[c]);
            return out[c];
        }

    private:
        // tanh-based soft saturation, unity small-signal gain
        static float sat (float x, float drive) noexcept { return std::tanh (x * drive) / drive; }

        static constexpr float lowDrive  = 0.22f;   // onset ≈ +8 dBFS on lows
        static constexpr float highDrive = 0.10f;   // highs stay clean longer

        DcBlocker hp;
        float lfSplit = 0.01f, hfRoll = 0.9f;
        std::array<float, kMaxChannels> lf {}, out {};
    };

    //==============================================================================
    class ClassAStage
    {
    public:
        ClassAStage() noexcept { setDrive (1.0f); }

        /** drive: 1 = nominal, >1 pushes harder (All mode). */
        void setDrive (float d) noexcept
        {
            k = baseK * juce::jmax (0.1f, d);
            const auto t = std::tanh (k * bias);
            offset = t;
            norm = 1.0f / (k * (1.0f - t * t));   // unity slope at x = 0
        }

        float process (float x) const noexcept
        {
            return (std::tanh (k * (x + bias)) - offset) * norm;
        }

    private:
        static constexpr float baseK = 0.55f;
        static constexpr float bias  = 0.18f;   // asymmetry -> 2nd harmonic
        float k = baseK, offset = 0.0f, norm = 1.0f;
    };
}
