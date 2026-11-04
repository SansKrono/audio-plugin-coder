#pragma once
/*  VaporStrings — octave layer [B] + square-up for source [A] (phase 2.1.4)
    Dependency-free (std only).

    PitchShifter   causal "rotating tape head": two taps sweep a delay line half a
                   grain apart, Hann-crossfaded (sin² + cos² = 1). Works on chords.
                   Also reused by the Slowdown FX (2.1.6) with a longer grain.
    Oversampler2x  linear-phase FIR half-band (31 taps, Kaiser β = 8) up/down pair.
                   Fixed latency: getLatency() base-rate samples.
    SquareUp       pre-emphasis → drive (0 … +48 dB) → tanh/hard-clip blend →
                   de-emphasis → level compensation, all at 2× rate.            */

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vs
{
//==============================================================================
class PitchShifter
{
public:
    void prepare (double sampleRate, float grainMs, float maxGrainMs = 0.0f)
    {
        sr = sampleRate;
        const float maxMs = std::max (grainMs, maxGrainMs);
        size = 1;
        while (size < (int) (sr * 0.001 * maxMs) + 8) size <<= 1;
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        setGrainMs (grainMs);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        write = 0;
        phase = 0.0f;
    }

    void setGrainMs (float ms) { grain = std::clamp ((float) (sr * 0.001 * ms), 64.0f, (float) (size - 4)); }

    /** ratio: 0.5 = octave down, 2 = octave up, 1 = unity. */
    void setRatio (float r) noexcept { ratio = std::clamp (r, 0.25f, 4.0f); }

    float process (float x) noexcept
    {
        buf[(size_t) write] = x;

        // phase advances so the read head moves at `ratio` × the write head
        phase += (1.0f - ratio) / grain;
        phase -= std::floor (phase);

        float p2 = phase + 0.5f;
        p2 -= std::floor (p2);

        const float y = read (phase) * hann (phase) + read (p2) * hann (p2);
        write = (write + 1) & mask;
        return y;
    }

private:
    static float hann (float p) noexcept { const float s = std::sin (3.14159265f * p); return s * s; }

    float read (float p) const noexcept
    {
        const float d = 1.0f + p * grain;              // delay in samples (≥ 1)
        float pos = (float) write - d;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos;
        const float f = pos - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + f * (b - a);
    }

    double sr = 48000.0;
    std::vector<float> buf;
    int size = 0, mask = 0, write = 0;
    float grain = 1920.0f, phase = 0.0f, ratio = 1.0f;
};

//==============================================================================
class Oversampler2x
{
public:
    static constexpr int kTaps = 31;           // odd; centre tap = 0.5 (half-band)

    Oversampler2x()
    {
        // windowed-sinc half-band, cutoff fs/4 (at the 2× rate), Kaiser window
        const int c = (kTaps - 1) / 2;
        const double beta = 8.0;
        double sum = 0.0;
        for (int n = 0; n < kTaps; ++n)
        {
            const int k = n - c;
            const double sinc = k == 0 ? 0.5 : std::sin (3.14159265358979 * k / 2.0) / (3.14159265358979 * k);
            const double r = (double) k / c;
            const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (beta);
            h[(size_t) n] = (float) (sinc * w);
            sum += h[(size_t) n];
        }
        for (auto& v : h) v = (float) (v / sum);  // unity DC gain
        reset();
    }

    void reset() { upHist.fill (0.0f); dnHist.fill (0.0f); upPos = dnPos = 0; }

    /** Base-rate latency of an up → down round trip. */
    static constexpr int getLatency() { return (kTaps - 1) / 2; }

    /** Upsample one sample into two (zero-stuffing + FIR, gain 2). */
    void up (float x, float& y0, float& y1) noexcept
    {
        y0 = fir (upHist, upPos, 2.0f * x);
        y1 = fir (upHist, upPos, 0.0f);
    }

    /** Downsample two 2×-rate samples into one (FIR, then decimate). The even
        phase is kept so the round trip is exactly getLatency() base samples
        (keeping the odd phase would add half a sample and break the dry match). */
    float down (float x0, float x1) noexcept
    {
        const float y = fir (dnHist, dnPos, x0);
        push (dnHist, dnPos, x1);
        return y;
    }

private:
    static void push (std::array<float, kTaps>& hist, int& pos, float x) noexcept
    {
        hist[(size_t) pos] = x;
        pos = (pos + 1) % kTaps;
    }

    float fir (std::array<float, kTaps>& hist, int& pos, float x) noexcept
    {
        hist[(size_t) pos] = x;
        float acc = 0.0f;
        int idx = pos;
        for (int n = 0; n < kTaps; ++n)
        {
            acc += h[(size_t) n] * hist[(size_t) idx];
            idx = idx == 0 ? kTaps - 1 : idx - 1;
        }
        pos = (pos + 1) % kTaps;
        return acc;
    }

    static double bessel0 (double x)
    {
        double s = 1.0, t = 1.0;
        for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; }
        return s;
    }

    std::array<float, kTaps> h {}, upHist {}, dnHist {};
    int upPos = 0, dnPos = 0;
};

//==============================================================================
class SquareUp
{
public:
    void reset() { os.reset(); preZ = deZ = 0.0f; }

    /** amount 0..1. At 0 the signal passes through linearly (only the OS latency). */
    void setAmount (float a) noexcept { amount = std::clamp (a, 0.0f, 1.0f); }

    static constexpr int getLatency() { return Oversampler2x::getLatency(); }

    float process (float x) noexcept
    {
        float u0, u1;
        os.up (x, u0, u1);
        return os.down (shape (u0), shape (u1));
    }

private:
    float shape (float x) noexcept
    {
        // pre-emphasis (lifts upper partials so the clipper keeps string brightness).
        // State updates even at amount 0 so engaging square-up never clicks.
        const float e = x - kEmph * preZ;
        preZ = x;

        if (amount <= 0.0f)
        {
            deZ = x;   // de-emphasis output equals the clean input here
            return x;
        }

        const float drive = std::pow (10.0f, 2.4f * amount);          // 0 … +48 dB
        const float v = e * drive;
        const float soft = std::tanh (v);
        const float hard = std::clamp (v, -1.0f, 1.0f);
        float s = soft + amount * (hard - soft);                        // tanh → hard clip

        // de-emphasis: exact inverse of the pre-emphasis (DC gain 1/(1-k) undoes (1-k))
        s = s + kEmph * deZ;
        deZ = s;

        // level compensation toward a DI-like peak level, blended in over the first 25 %
        const float wet = s * kOutLevel;
        const float blend = std::min (1.0f, amount * 4.0f);
        return x + blend * (wet - x);
    }

    static constexpr float kEmph = 0.6f;
    static constexpr float kOutLevel = 0.3f;   // de-emphasised ±1 square peaks ≈ ±2.5 → ≈ 0.75

    Oversampler2x os;
    float amount = 0.0f, preZ = 0.0f, deZ = 0.0f;
};

} // namespace vs
