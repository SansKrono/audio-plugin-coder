#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace anvil
{
    /** 07 Ceiling: lookahead true-peak limiter (architecture.md).

        Detection: 4× polyphase windowed-sinc interpolation of the input
        (kNumTaps per phase, group delay kInterpDelay samples) plus parabolic
        refinement around local extrema → per-sample linked true-peak estimate.

        Gain: required gain r = min (1, ceiling / peak) → instant-attack /
        150 ms release → sliding minimum over H samples → moving average over
        B = W + 1 samples, with the audio delayed by W + kInterpDelay + kGuard.
        The min-hold is 2·kGuard samples longer than the average, so every
        output sample within ±kGuard of a detected inter-sample peak gets
        gain <= r. ±1 would cover the peak itself; the wider guard keeps the
        neighbours that shape the reconstructed (inter-sample) waveform from
        being louder than the peak's gain allows. Gain changes stay smooth.

        Latency = W + kInterpDelay + kGuard, W = 1.5 ms. No allocation after prepare. */
    class TruePeakLimiter
    {
    public:
        static constexpr int kNumTaps     = 32;
        static constexpr int kInterpDelay = kNumTaps / 2;
        static constexpr int kPhases      = 4;
        static constexpr float kLookaheadMs = 1.5f;
        static constexpr int kGuard = 8;   // samples each side of a detected peak held at <= r

        static int lookaheadSamples (double fs) { return juce::jmax (1, (int) std::lround (kLookaheadMs * 0.001 * fs)); }
        static int latencySamples (double fs)   { return lookaheadSamples (fs) + kInterpDelay + kGuard; }

        void prepare (double sampleRate, float releaseMs)
        {
            fs = sampleRate;
            W = lookaheadSamples (fs);
            H = W + 1 + 2 * kGuard;
            B = W + 1;
            A = latencySamples (fs);
            relCoef = (float) std::exp (-1.0 / (0.001 * releaseMs * fs));

            buildInterpolator();

            for (int c = 0; c < 2; ++c)
            {
                hist[c].assign ((size_t) kNumTaps, 0.0f);
                delay[c].assign ((size_t) A, 0.0f);   // ring of A slots: read the sample written A steps ago
            }
            minVal.assign ((size_t) H + 1, 1.0f);
            minIdx.assign ((size_t) H + 1, 0);
            box.assign ((size_t) B, 1.0);

            ceiling.reset (fs, 0.02);
            ceiling.setCurrentAndTargetValue (std::pow (10.0f, -0.3f / 20.0f));
            reset();
        }

        void reset()
        {
            for (int c = 0; c < 2; ++c)
            {
                std::fill (hist[c].begin(), hist[c].end(), 0.0f);
                std::fill (delay[c].begin(), delay[c].end(), 0.0f);
                prev1[c] = prev2[c] = 0.0f;
            }
            histPos = delayPos = 0;
            released = 1.0f;
            qHead = qTail = 0;
            counter = 0;
            std::fill (box.begin(), box.end(), 1.0);
            boxSum = (double) B;
            boxPos = 0;
        }

        void snap() { ceiling.setCurrentAndTargetValue (ceiling.getTargetValue()); }

        void setTargets (float ceilingDb) { ceiling.setTargetValue (std::pow (10.0f, ceilingDb / 20.0f)); }

        int getLatency() const noexcept { return A; }

        /** Returns the max gain reduction (dB, >= 0) applied in this call. */
        float process (float* const* ch, int numCh, int n) noexcept
        {
            float minGain = 1.0f;
            for (int i = 0; i < n; ++i)
            {
                const float ceil = ceiling.getNextValue();

                // ---- detection: linked true-peak estimate of the newest input ----
                float peak = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    hist[c][(size_t) histPos] = ch[c][i];
                    peak = std::max (peak, detect (c));
                }
                histPos = (histPos + 1) % kNumTaps;

                // ---- gain computer ----
                const float r = peak > ceil ? ceil / peak : 1.0f;
                released = r < released ? r : r + relCoef * (released - r);
                const float hold = slidingMin (released);
                boxSum += (double) hold - box[(size_t) boxPos];
                box[(size_t) boxPos] = hold;
                boxPos = (boxPos + 1) % B;
                const float g = std::min (1.0f, (float) (boxSum / (double) B));
                minGain = std::min (minGain, g);

                // ---- delayed audio × gain ----
                for (int c = 0; c < numCh; ++c)
                {
                    auto& d = delay[c];
                    const float x = ch[c][i];
                    ch[c][i] = d[(size_t) delayPos] * g;
                    d[(size_t) delayPos] = x;
                }
                delayPos = (delayPos + 1) % A;
                ++counter;
            }
            return minGain < 1.0f ? -20.0f * std::log10 (std::max (minGain, 1.0e-9f)) : 0.0f;
        }

    private:
        /** Interpolated 4× points between hist samples (n − D) and (n − D + 1) of channel c;
            returns the largest |value| including parabolic refinement. */
        float detect (int c) noexcept
        {
            float pk = 0.0f;
            for (int k = 0; k < kPhases; ++k)
            {
                float v;
                if (k == 0)
                {
                    v = histAt (c, kInterpDelay);
                }
                else
                {
                    v = 0.0f;
                    const auto& h = coef[(size_t) k];
                    for (int t = 0; t < kNumTaps; ++t)
                        v += h[(size_t) t] * histAt (c, t);
                }

                pk = std::max (pk, std::abs (v));

                // parabolic refinement when the previous point is a local extremum
                const float y0 = prev2[c], y1 = prev1[c], y2 = v;
                if ((y1 - y0) * (y1 - y2) > 0.0f)
                {
                    const float denom = y0 - 2.0f * y1 + y2;
                    if (std::abs (denom) > 1.0e-12f)
                    {
                        const float p = 0.5f * (y0 - y2) / denom;
                        if (std::abs (p) <= 1.0f)
                            pk = std::max (pk, std::abs (y1 - 0.25f * (y0 - y2) * p));
                    }
                }
                prev2[c] = prev1[c];
                prev1[c] = v;
            }
            return pk;
        }

        /** x[n − back] for channel c (back = 0 is the newest sample). */
        float histAt (int c, int back) const noexcept
        {
            int idx = histPos - back;
            if (idx < 0) idx += kNumTaps;
            return hist[c][(size_t) idx];
        }

        /** Monotonic-queue sliding minimum over the last H values. */
        float slidingMin (float v) noexcept
        {
            const int cap = H + 1;
            while (qHead != qTail)
            {
                const int last = (qTail - 1 + cap) % cap;
                if (minVal[(size_t) last] >= v) qTail = last; else break;
            }
            minVal[(size_t) qTail] = v;
            minIdx[(size_t) qTail] = counter;
            qTail = (qTail + 1) % cap;
            while (minIdx[(size_t) qHead] <= counter - H)
                qHead = (qHead + 1) % cap;
            return minVal[(size_t) qHead];
        }

        void buildInterpolator()
        {
            // value at t = (n − D) + k/4 from x[n − t], t = 0..N−1  →  sinc (t − D + k/4) · Kaiser
            const double beta = 8.0, halfSpan = (double) kInterpDelay + 0.5;
            const auto bessel0 = [] (double x)
            {
                double sum = 1.0, term = 1.0;
                for (int m = 1; m < 30; ++m) { term *= (x / (2.0 * m)) * (x / (2.0 * m)); sum += term; }
                return sum;
            };
            for (int k = 0; k < kPhases; ++k)
            {
                double total = 0.0;
                for (int t = 0; t < kNumTaps; ++t)
                {
                    const double u = (double) t - kInterpDelay + (double) k / kPhases;
                    const double sinc = std::abs (u) < 1.0e-12 ? 1.0 : std::sin (juce::MathConstants<double>::pi * u) / (juce::MathConstants<double>::pi * u);
                    const double r = u / halfSpan;
                    const double w = std::abs (r) >= 1.0 ? 0.0 : bessel0 (beta * std::sqrt (1.0 - r * r)) / bessel0 (beta);
                    coef[(size_t) k][(size_t) t] = (float) (sinc * w);
                    total += sinc * w;
                }
                for (auto& h : coef[(size_t) k]) h = (float) (h / total);   // unity DC gain per phase
            }
        }

        double fs = 48000.0;
        int W = 72, H = 75, B = 73, A = 89;
        float relCoef = 0.0f, released = 1.0f;

        std::array<std::array<float, kNumTaps>, kPhases> coef {};
        std::vector<float> hist[2], delay[2];
        float prev1[2] {}, prev2[2] {};
        int histPos = 0, delayPos = 0;

        std::vector<float> minVal;
        std::vector<juce::int64> minIdx;
        int qHead = 0, qTail = 0;
        juce::int64 counter = 0;

        std::vector<double> box;
        double boxSum = 0.0;
        int boxPos = 0;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ceiling;
    };
}
