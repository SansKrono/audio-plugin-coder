#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include "BandSplitter3.h"
#include "ParameterIDs.hpp"

#include <algorithm>
#include <cmath>

/*  RitualAnvil DSP stages (architecture.md "Core Components").
    Every stage: prepare (fs, maxBlock) → setTargets (...) once per block →
    process (channels, numChannels, numSamples). Up to 2 channels, in place,
    per-sample smoothing, no allocation after prepare. Coefficient updates run
    every `coeffInterval` samples on a counter that persists across blocks, so
    the output does not depend on host block size. */

namespace anvil
{
    static constexpr int coeffInterval = 32;

    inline float dbToGain (float db) noexcept { return std::exp (db * 0.11512925464970229f); }   // ln(10)/20
    inline float gainToDb (float g) noexcept  { return g > 1.0e-9f ? 20.0f * std::log10 (g) : -180.0f; }
    inline float msToCoef (double fs, float ms) noexcept { return (float) std::exp (-1.0 / (0.001 * ms * fs)); }

    using Linear = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    using Multiplicative = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;

    template <typename Smoothed>
    void snapTo (Smoothed& v) noexcept { v.setCurrentAndTargetValue (v.getTargetValue()); }

    //==========================================================================
    // 01 Input: trim gain → 12 dB/oct HPF (Q 0.707)
    //==========================================================================
    class InputStage
    {
    public:
        void prepare (double sampleRate)
        {
            fs = sampleRate;
            gain.reset (fs, 0.02);
            hpfHz.reset (fs, 0.05);
            gain.setCurrentAndTargetValue (1.0f);
            hpfHz.setCurrentAndTargetValue (40.0f);
            lastHz = -1.0f;
            countdown = 0;
            hpf.reset();
        }

        void reset() { hpf.reset(); }
        void snap() { snapTo (gain); snapTo (hpfHz); }

        void setTargets (float gainDb, float hz)
        {
            gain.setTargetValue (dbToGain (gainDb));
            hpfHz.setTargetValue (hz);
        }

        void process (float* const* ch, int numCh, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = gain.getNextValue();
                const float f = hpfHz.getNextValue();
                if (--countdown <= 0)
                {
                    countdown = coeffInterval;
                    if (std::abs (f - lastHz) > 1.0e-4f) { hpf.setCoeffs (BiquadCoeffs::highPass (fs, f, 0.7071)); lastHz = f; }
                }
                for (int c = 0; c < numCh; ++c)
                    ch[c][i] = hpf.process (c, ch[c][i] * g);
            }
        }

    private:
        double fs = 48000.0;
        Linear gain;
        Multiplicative hpfHz;
        Biquad hpf;
        float lastHz = -1.0f;
        int countdown = 0;
    };

    //==========================================================================
    // 02 Transient: split 300 Hz / 20 kHz, fast/slow envelope shaper on low + mid
    //    (port of POC Transient_Shaper, detector stereo-linked per band)
    //==========================================================================
    class TransientStage
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            fs = sampleRate;
            splitter.prepare (fs, maxBlock, fixed::transientSplitLow, fixed::transientSplitHigh);
            fastAtt = msToCoef (fs, 1.0f);   fastRel = msToCoef (fs, 30.0f);
            slowAtt = msToCoef (fs, 20.0f);  slowRel = msToCoef (fs, 200.0f);
            attackGain.reset (fs, 0.02);
            sustainGain.reset (fs, 0.02);
            attackGain.setCurrentAndTargetValue (0.0f);
            sustainGain.setCurrentAndTargetValue (0.0f);
            reset();
        }

        void reset()
        {
            splitter.reset();
            for (auto& b : env) b = {};
        }

        void snap() { snapTo (attackGain); snapTo (sustainGain); }

        void setTargets (float attackDb, float sustainDb)
        {
            attackGain.setTargetValue (dbToGain (attackDb) - 1.0f);
            sustainGain.setTargetValue (dbToGain (sustainDb) - 1.0f);
        }

        void process (float* const* ch, int numCh, int n) noexcept
        {
            float lo[2] {}, mi[2] {}, hi[2] {};
            for (int i = 0; i < n; ++i)
            {
                const float aG = attackGain.getNextValue();
                const float sG = sustainGain.getNextValue();

                for (int c = 0; c < numCh; ++c)
                    splitter.process (c, ch[c][i], lo[c], mi[c], hi[c]);

                const float gLo = env[0].gain (peak (lo, numCh), aG, sG, *this);
                const float gMi = env[1].gain (peak (mi, numCh), aG, sG, *this);

                for (int c = 0; c < numCh; ++c)
                    ch[c][i] = lo[c] * gLo + mi[c] * gMi + hi[c];
            }
        }

    private:
        struct Envelope
        {
            float fast = 0.0f, slow = 0.0f;

            float gain (float x, float aG, float sG, const TransientStage& s) noexcept
            {
                fast = x + (x > fast ? s.fastAtt : s.fastRel) * (fast - x);
                slow = x + (x > slow ? s.slowAtt : s.slowRel) * (slow - x);
                const float tc = fast > 1.0e-6f ? std::clamp ((fast - slow) / fast, 0.0f, 1.0f) : 0.0f;
                return 1.0f + aG * tc + sG * (1.0f - tc);
            }
        };

        static float peak (const float* v, int numCh) noexcept
        {
            return numCh > 1 ? std::max (std::abs (v[0]), std::abs (v[1])) : std::abs (v[0]);
        }

        double fs = 48000.0;
        BandSplitter3 splitter;
        Envelope env[2];
        float fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0;
        Linear attackGain, sustainGain;
    };

    //==========================================================================
    // 03 Tone EQ: 5 fixed peaking bands
    //==========================================================================
    class ToneEQ
    {
    public:
        static constexpr int numBands = 5;

        void prepare (double sampleRate)
        {
            fs = sampleRate;
            for (int b = 0; b < numBands; ++b)
            {
                gainDb[b].reset (fs, 0.05);
                gainDb[b].setCurrentAndTargetValue (0.0f);
                lastDb[b] = 1.0e9f;
            }
            countdown = 0;
            reset();
        }

        void reset() { for (auto& f : filters) f.reset(); }
        void snap() { for (auto& g : gainDb) snapTo (g); }

        void setTargets (const float (&db)[numBands])
        {
            for (int b = 0; b < numBands; ++b)
                gainDb[b].setTargetValue (db[b]);
        }

        void process (float* const* ch, int numCh, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                float g[numBands];
                for (int b = 0; b < numBands; ++b) g[b] = gainDb[b].getNextValue();

                if (--countdown <= 0)
                {
                    countdown = coeffInterval;
                    for (int b = 0; b < numBands; ++b)
                    {
                        if (std::abs (g[b] - lastDb[b]) > 1.0e-5f)
                        {
                            filters[b].setCoeffs (BiquadCoeffs::peak (fs, fixed::eqFreq[b], fixed::eqQ[b], g[b]));
                            lastDb[b] = g[b];
                        }
                    }
                }

                for (int c = 0; c < numCh; ++c)
                {
                    float x = ch[c][i];
                    for (int b = 0; b < numBands; ++b)
                        x = filters[b].process (c, x);   // 0 dB peak == identity; keep running for state continuity
                    ch[c][i] = x;
                }
            }
        }

    private:
        double fs = 48000.0;
        Linear gainDb[numBands];
        Biquad filters[numBands];
        float lastDb[numBands] {};
        int countdown = 0;
    };

    //==========================================================================
    // 04 Punch: parallel compression (−20 dB, 8:1, 3/150 ms), blend 0..0.5
    //==========================================================================
    class PunchStage
    {
    public:
        void prepare (double sampleRate)
        {
            fs = sampleRate;
            attCoef = msToCoef (fs, fixed::punchAttackMs);
            relCoef = msToCoef (fs, fixed::punchReleaseMs);
            blend.reset (fs, 0.02);
            blend.setCurrentAndTargetValue (0.0f);
            reset();
        }

        void reset() { grDb = 0.0f; }
        void snap() { snapTo (blend); }

        void setTargets (float blend01) { blend.setTargetValue (std::clamp (blend01, 0.0f, fixed::punchMaxBlend)); }

        /** Returns the max gain reduction (dB, >= 0) seen in this call while the blend is > 0. */
        float process (float* const* ch, int numCh, int n) noexcept
        {
            constexpr float slope = 1.0f - 1.0f / fixed::punchRatio;
            float maxGr = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float b = blend.getNextValue();
                float lvl = std::abs (ch[0][i]);
                if (numCh > 1) lvl = std::max (lvl, std::abs (ch[1][i]));

                const float target = lvl > thresholdGain ? (gainToDb (lvl) - fixed::punchThresholdDb) * slope : 0.0f;
                grDb = target + (target > grDb ? attCoef : relCoef) * (grDb - target);

                if (b > 1.0e-6f)
                {
                    const float wetGain = grDb > 1.0e-4f ? dbToGain (-grDb) : 1.0f;
                    const float mix = (1.0f - b) + b * wetGain;   // dry·(1−b) + wet·b, wet = x·wetGain
                    for (int c = 0; c < numCh; ++c)
                        ch[c][i] *= mix;
                    maxGr = std::max (maxGr, grDb);
                }
            }
            return maxGr;
        }

    private:
        double fs = 48000.0;
        float attCoef = 0, relCoef = 0, grDb = 0;
        const float thresholdGain = std::pow (10.0f, fixed::punchThresholdDb / 20.0f);
        Linear blend;
    };

    //==========================================================================
    // 05 Heat: 4× oversampled (FIR half-band, linear phase, integer latency).
    //    At the OS rate: split 200 Hz / 5 kHz → hard / soft (tanh) / tube (asym) → sum.
    //    The OS path always runs so latency and phase never change with Drive.
    //==========================================================================
    class HeatStage
    {
    public:
        static constexpr size_t osOrder = 2;   // 2^2 = 4×

        static std::unique_ptr<juce::dsp::Oversampling<float>> makeOversampler (int numChannels)
        {
            return std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) numChannels, osOrder,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                true,    // max quality
                true);   // integer latency
        }

        /** Latency in base-rate samples (independent of sample rate). */
        static int latencySamples()
        {
            auto probe = makeOversampler (1);
            probe->initProcessing (1);
            return (int) std::lround (probe->getLatencyInSamples());
        }

        void prepare (double sampleRate, int maxBlock, int numChannels)
        {
            os = makeOversampler (juce::jlimit (1, 2, numChannels));
            os->initProcessing ((size_t) juce::jmax (1, maxBlock));
            osRate = sampleRate * (double) os->getOversamplingFactor();

            splitter.prepare (osRate, maxBlock * (int) os->getOversamplingFactor(), fixed::heatSplitLow, fixed::heatSplitHigh);
            dcBlock.setCutoff (osRate, 5.0);
            driveDb.reset (osRate, 0.02);
            driveDb.setCurrentAndTargetValue (0.0f);
            gainsDirty = true;
            reset();
        }

        void reset()
        {
            if (os != nullptr) os->reset();
            splitter.reset();
            dcBlock.reset();
        }

        void snap() { snapTo (driveDb); gainsDirty = true; }

        void setTargets (float db) { driveDb.setTargetValue (std::clamp (db, 0.0f, 20.0f)); }

        static float hardClip (float x) noexcept { return std::clamp (x, -1.0f, 1.0f); }
        static float softClip (float x) noexcept { return std::tanh (x); }
        static float tube (float x) noexcept     { return x > 0.0f ? std::tanh (x) : 0.5f * std::tanh (2.0f * x); }

        void process (float* const* ch, int numCh, int n) noexcept
        {
            juce::dsp::AudioBlock<float> block (ch, (size_t) numCh, (size_t) n);
            auto up = os->processSamplesUp (block);
            const auto upN = (int) up.getNumSamples();
            float* upCh[2] { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };

            float lo = 0, mi = 0, hi = 0;
            for (int i = 0; i < upN; ++i)
            {
                const bool moving = driveDb.isSmoothing();
                const float d = driveDb.getNextValue();
                if (moving || gainsDirty)   // gains only change while Drive moves
                {
                    gainsDirty = false;
                    amount = std::clamp (d / 3.0f, 0.0f, 1.0f);   // fade the shapers in over the first 3 dB
                    g = dbToGain (d);
                    comp = dbToGain (-0.5f * d);                  // rough output compensation (tuned in 4.1.3)
                }

                for (int c = 0; c < numCh; ++c)
                {
                    splitter.process (c, upCh[c][i], lo, mi, hi);
                    const float dry = lo + mi + hi;
                    float y = dry;
                    if (amount > 0.0f)
                    {
                        const float shaped = (hardClip (lo * g) + softClip (mi * g) + tube (hi * g)) * comp;
                        y = dry + amount * (shaped - dry);
                    }
                    upCh[c][i] = dcBlock.process (c, y);
                }
            }

            os->processSamplesDown (block);
        }

    private:
        double osRate = 192000.0;
        bool gainsDirty = true;
        float amount = 0.0f, g = 1.0f, comp = 1.0f;
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        BandSplitter3 splitter;
        OnePoleHP dcBlock;
        Linear driveDb;
    };

    //==========================================================================
    // 06 Width: M/S, side gain applied above 200 Hz only (side below 200 Hz untouched,
    //    so width 100 % is an exact identity, unlike the POC's side HPF)
    //==========================================================================
    class WidthStage
    {
    public:
        void prepare (double sampleRate)
        {
            sideSplit.setCutoff (sampleRate, fixed::widthSideSplitHz);
            width.reset (sampleRate, 0.02);
            width.setCurrentAndTargetValue (1.0f);
            reset();
        }

        void reset() { sideSplit.reset(); }
        void snap() { snapTo (width); }

        void setTargets (float widthPercent) { width.setTargetValue (widthPercent / 100.0f); }

        void process (float* const* ch, int numCh, int n) noexcept
        {
            if (numCh < 2) { width.skip (n); return; }
            for (int i = 0; i < n; ++i)
            {
                const float w = width.getNextValue();
                const float l = ch[0][i], r = ch[1][i];
                const float m = 0.5f * (l + r);
                const float s = 0.5f * (l - r);
                const float sHigh = sideSplit.process (0, s);
                const float sOut = (s - sHigh) + sHigh * w;
                ch[0][i] = m + sOut;
                ch[1][i] = m - sOut;
            }
        }

    private:
        OnePoleHP sideSplit;
        Linear width;
    };
}
