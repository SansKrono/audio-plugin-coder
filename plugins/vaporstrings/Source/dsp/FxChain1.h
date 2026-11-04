#pragma once
/*  VaporStrings — FX chain I (phase 2.1.6). Dependency-free (std only), stereo.

    Bypass        10 ms equal-gain crossfade between input and FX output; a fully
                  bypassed FX is skipped (and reset before it fades back in).
    Slowdown      per-channel rotating-head shifter (60 ms grain) at slow_pitch +
                  a smoothed random drift (0.2 Hz, ±30 ct × slow_drift).
    Drive         Tape / Tube / Fuzz at 2× (Oversampler2x), Bitcrush at 1×;
                  tilt-EQ tone after; level-compensated.
    Ensemble      string-machine BBD model: 3 modulated delays (5/7/10 ms), each LFO
                  = slow sine (rate) + fast sine (rate × 9.5, 0.3 depth), 0/120/240°;
                  Chorus I / Chorus II / Ensemble line + pan layouts; 9 kHz BBD LPF.
    Wobble/Age    one tape transport for both channels: modulated 8 ms delay
                  (wow random walk + flutter), age = LPF 20k→3.5k + saturation +
                  pink hiss, Poisson dropouts (up to −24 dB, 30–200 ms).        */

#include "Analysis.h"   // Biquad
#include "Shifter.h"    // PitchShifter, Oversampler2x

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vs
{
//==============================================================================
struct Rng
{
    uint32_t s = 0x12345678u;
    float uni() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) / 8388608.0f - 1.0f; } // -1..1
    float pos() noexcept { return 0.5f * (uni() + 1.0f); }                                                         //  0..1
};

/** One-pole parameter smoother (per sample). */
struct Smooth
{
    float v = 0.0f, t = 0.0f, c = 0.999f;
    void prepare (double sr, float ms) { c = std::exp (-1.0f / (0.001f * ms * (float) sr)); }
    void snap (float x) { v = t = x; }
    float next() noexcept { v = t + c * (v - t); return v; }
};

/** Fractional-delay line (linear interpolation), power-of-two ring. */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size = 1;
        while (size < maxSamples + 4) size <<= 1;
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        pos = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    void push (float x) noexcept { buf[(size_t) pos] = x; pos = (pos + 1) & mask; }
    /** delay in samples (≥ 1) behind the most recent push */
    float read (float d) const noexcept
    {
        float p = (float) pos - d;
        while (p < 0.0f) p += (float) size;
        const int i0 = (int) p;
        const float f = p - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + f * (b - a);
    }
private:
    std::vector<float> buf;
    int size = 0, mask = 0, pos = 0;
};

//==============================================================================
/** Shared on/off crossfade. Call begin() per block, then mix() per sample. */
class Bypass
{
public:
    void prepare (double sr) { step = 1.0f / (0.010f * (float) sr); g = 0.0f; }
    /** Returns true when the FX must be processed this block. wasOff → caller resets FX state. */
    bool begin (bool on, bool& wasOff) noexcept
    {
        target = on ? 1.0f : 0.0f;
        wasOff = g <= 0.0f && on;
        return on || g > 0.0f;
    }
    float next() noexcept
    {
        g = target > g ? std::min (target, g + step) : std::max (target, g - step);
        return g;
    }
private:
    float g = 0.0f, target = 0.0f, step = 0.002f;
};

/** Run one insert FX over a stereo block with its bypass crossfade. */
template <class Fx>
inline void runInsert (Bypass& bp, bool on, Fx& fx, float* l, float* r, int n) noexcept
{
    bool wasOff = false;
    if (! bp.begin (on, wasOff))
        return;
    if (wasOff) { fx.reset(); fx.snap(); }
    for (int i = 0; i < n; ++i)
    {
        float pl = l[i], pr = r[i];
        fx.process (pl, pr);
        const float g = bp.next();
        l[i] += g * (pl - l[i]);
        r[i] += g * (pr - r[i]);
    }
}

//==============================================================================
class Slowdown
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& s : sh) s.prepare (sr, 60.0f);
        driftLp = std::exp (-1.0f / (0.25f * (float) sr));   // smooths the random walk
        reset();
    }
    void reset() { for (auto& s : sh) s.reset(); drift = driftTarget = 0.0f; driftTimer = 0; }

    void setParams (float semitones, float driftAmt) noexcept { st = semitones; amt = std::clamp (driftAmt, 0.0f, 1.0f); }
    void snap() {}

    void process (float& l, float& r) noexcept
    {
        // 0.2 Hz random drift: a new target every 5 s / 0.2 Hz cycle, smoothed
        if (--driftTimer <= 0) { driftTarget = rng.uni(); driftTimer = (int) (sr / 0.2 * 0.5); }
        drift = driftTarget + driftLp * (drift - driftTarget);
        const float ratio = std::exp2 ((st + drift * amt * 0.30f) / 12.0f);
        sh[0].setRatio (ratio);
        sh[1].setRatio (ratio);
        l = sh[0].process (l);
        r = sh[1].process (r);
    }

private:
    double sr = 48000.0;
    std::array<PitchShifter, 2> sh;
    Rng rng;
    float st = -3.0f, amt = 0.1f, drift = 0.0f, driftTarget = 0.0f, driftLp = 0.999f;
    int driftTimer = 0;
};

//==============================================================================
class Drive
{
public:
    enum class Mode { Tape = 0, Tube, Fuzz, Bitcrush };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        amt.prepare (sr, 20.0f);
        tone.prepare (sr, 20.0f);
        for (auto& b : fuzzBump) b.setLowPass (2.0 * sr, 800.0, 0.7);   // band below via LP; bump = x + k·LP
        tiltA = std::exp (-2.0f * 3.14159265f * 1000.0f / (float) sr);
        reset();
    }
    void reset()
    {
        for (auto& o : os) o.reset();
        for (auto& b : fuzzBump) b.reset();
        tiltZ = { 0.0f, 0.0f }; dcX = dcY = { 0.0f, 0.0f };
        holdCount = 0; held = { 0.0f, 0.0f };
    }
    void setParams (Mode m, float amount, float toneAmt) noexcept { mode = m; amt.t = std::clamp (amount, 0.0f, 1.0f); tone.t = std::clamp (toneAmt, 0.0f, 1.0f); }
    void snap() { amt.snap (amt.t); tone.snap (tone.t); }

    static constexpr int getLatency() { return Oversampler2x::getLatency(); }

    void process (float& l, float& r) noexcept
    {
        const float a = amt.next();
        const float t = tone.next();
        float io[2] = { l, r };

        if (mode == Mode::Bitcrush)
        {
            const float bits = 16.0f - 13.0f * a;                 // 16 → 3 bits
            const float levels = std::exp2 (bits - 1.0f);
            const int hold = 1 + (int) (a * a * 15.0f);           // 1× → 16× sample-and-hold
            if (++holdCount >= hold) { holdCount = 0; held = { io[0], io[1] }; }
            for (int c = 0; c < 2; ++c) io[c] = std::round (held[(size_t) c] * levels) / levels;
        }
        else
        {
            const float g = std::pow (10.0f, 1.5f * a);           // 0 … +30 dB
            const float comp = 1.0f / std::sqrt (g);
            for (int c = 0; c < 2; ++c)
            {
                float u0, u1;
                os[(size_t) c].up (io[c], u0, u1);
                u0 = shape (u0, g, c);
                u1 = shape (u1, g, c);
                io[c] = os[(size_t) c].down (u0, u1) * comp;
            }
        }

        // tilt EQ around 1 kHz: ±6 dB per band, a 12 dB swing end to end (tone 0 = dark, 1 = bright)
        const float tiltDb = (t - 0.5f) * 12.0f;
        const float hiG = std::pow (10.0f, tiltDb / 20.0f), loG = 1.0f / hiG;
        for (int c = 0; c < 2; ++c)
        {
            auto& z = tiltZ[(size_t) c];
            z = io[c] + tiltA * (z - io[c]);                      // one-pole low band
            io[c] = z * loG + (io[c] - z) * hiG;
        }
        l = io[0];
        r = io[1];
    }

private:
    float shape (float x, float g, int c) noexcept
    {
        switch (mode)
        {
            case Mode::Tape: return std::tanh (x * g);
            case Mode::Tube:
            {
                const float v = x * g + 0.15f;                    // bias → even harmonics
                float y = v / (1.0f + std::abs (v)) * 1.4f;
                // DC blocker at the 2× rate
                const float out = y - dcX[(size_t) c] + 0.9995f * dcY[(size_t) c];
                dcX[(size_t) c] = y; dcY[(size_t) c] = out;
                return out;
            }
            case Mode::Fuzz:
            {
                const float bump = x + 1.5f * fuzzBump[(size_t) c].process (x);   // fat mids before the clip
                return std::clamp (bump * g * 1.5f, -1.0f, 1.0f);
            }
            case Mode::Bitcrush: break;
        }
        return x;
    }

    double sr = 48000.0;
    Mode mode = Mode::Tape;
    Smooth amt, tone;
    std::array<Oversampler2x, 2> os;
    std::array<Biquad, 2> fuzzBump;
    std::array<float, 2> tiltZ {}, dcX {}, dcY {}, held {};
    float tiltA = 0.9f;
    int holdCount = 0;
};

//==============================================================================
class Ensemble
{
public:
    enum class Mode { ChorusI = 0, ChorusII, Ensemble };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (0.025 * sr));
        for (auto& b : bbd) b.setLowPass (sr, 9000.0, 0.6);
        rate.prepare (sr, 50.0f); depth.prepare (sr, 50.0f); mix.prepare (sr, 20.0f);
        reset();
    }
    void reset() { for (auto& d : lines) d.reset(); for (auto& b : bbd) b.reset(); phase = 0.0; }
    void setParams (Mode m, float rateHz, float depthAmt, float mixAmt) noexcept
    {
        mode = m; rate.t = std::clamp (rateHz, 0.01f, 10.0f);
        depth.t = std::clamp (depthAmt, 0.0f, 1.0f); mix.t = std::clamp (mixAmt, 0.0f, 1.0f);
    }
    void snap() { rate.snap (rate.t); depth.snap (depth.t); mix.snap (mix.t); }

    void process (float& l, float& r) noexcept
    {
        const float in = 0.5f * (l + r);
        const float rt = rate.next(), dp = depth.next(), mx = mix.next();
        phase += rt / sr;
        phase -= std::floor (phase);

        static constexpr float base[3] = { 5.0f, 7.0f, 10.0f };       // ms
        float tap[3];
        for (int k = 0; k < 3; ++k)
        {
            const double ph = phase + k / 3.0;
            const float slow = (float) std::sin (6.283185307 * ph);
            const float fast = (float) std::sin (6.283185307 * (ph * 9.5 + k * 0.29));
            const float ms = base[k] + dp * (2.0f * slow + 0.3f * 0.6f * fast);
            lines[(size_t) k].push (in);
            tap[k] = lines[(size_t) k].read (std::max (1.0f, ms * 0.001f * (float) sr));
        }

        float wl, wr;
        switch (mode)
        {
            case Mode::ChorusI:  wl = tap[0];                          wr = tap[1];                          break;
            case Mode::ChorusII: wl = 0.7071f * (tap[0] + tap[2]);     wr = 0.7071f * (tap[1] + tap[2]);     break;
            default:             wl = 0.5774f * (tap[0] + 0.5f * tap[1] + 0.75f * tap[2]) * 1.15f;
                                 wr = 0.5774f * (0.5f * tap[0] + tap[1] + 0.75f * tap[2]) * 1.15f;      break;
        }
        wl = bbd[0].process (wl);
        wr = bbd[1].process (wr);
        l = l + mx * (wl - l);
        r = r + mx * (wr - r);
    }

private:
    double sr = 48000.0, phase = 0.0;
    Mode mode = Mode::Ensemble;
    std::array<DelayLine, 3> lines;
    std::array<Biquad, 2> bbd;
    Smooth rate, depth, mix;
};

//==============================================================================
class Wobble
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : dl) d.prepare ((int) (0.025 * sr));
        wow.prepare (sr, 50.0f); flutter.prepare (sr, 50.0f); age.prepare (sr, 50.0f); drop.prepare (sr, 50.0f);
        walkLp = std::exp (-1.0f / (0.35f * (float) sr));
        dipCoef = std::exp (-1.0f / (0.008f * (float) sr));           // 8 ms dropout edges
        reset();
    }
    void reset()
    {
        for (auto& d : dl) d.reset();
        for (auto& b : lp) b.reset();
        walk = walkTarget = 0.0f; walkTimer = 0; flPhase = 0.0;
        pink = {}; dipGain = 1.0f; dipTimer = 0; dipLeft = 0; dipDepth = 1.0f; lastLpHz = -1.0f;
    }
    void setParams (float wowAmt, float flutterAmt, float ageAmt, float dropAmt) noexcept
    {
        wow.t = std::clamp (wowAmt, 0.0f, 1.0f); flutter.t = std::clamp (flutterAmt, 0.0f, 1.0f);
        age.t = std::clamp (ageAmt, 0.0f, 1.0f); drop.t = std::clamp (dropAmt, 0.0f, 1.0f);
    }
    void snap() { wow.snap (wow.t); flutter.snap (flutter.t); age.snap (age.t); drop.snap (drop.t); }

    void process (float& l, float& r) noexcept
    {
        const float w = wow.next(), f = flutter.next(), a = age.next(), d = drop.next();

        // wow: smoothed random walk, new target every 0.3–1.5 Hz half-cycle
        if (--walkTimer <= 0) { walkTarget = rng.uni(); walkTimer = (int) (sr * (0.33 + 1.3 * rng.pos())); }
        walk = walkTarget + walkLp * (walk - walkTarget);
        // flutter: 6–12 Hz sine (rate drifts with the walk) + jitter
        flPhase += (9.0 + 3.0 * walk) / sr;
        flPhase -= std::floor (flPhase);
        const float fl = (float) std::sin (6.283185307 * flPhase) + 0.25f * rng.uni();
        const float ms = 8.0f + 3.0f * w * walk + 0.3f * f * fl;
        const float del = std::max (1.0f, ms * 0.001f * (float) sr);

        dl[0].push (l); dl[1].push (r);
        float io[2] = { dl[0].read (del), dl[1].read (del) };

        // age: LPF 20 kHz → 3.5 kHz (updated when it moves), saturation, pink hiss
        const float lpHz = 20000.0f * std::pow (3500.0f / 20000.0f, a);
        if (std::abs (lpHz - lastLpHz) > 1.0f) { for (auto& b : lp) b.setLowPass (sr, std::min (lpHz, (float) (0.45 * sr)), 0.7071); lastLpHz = lpHz; }
        const float hissAmp = a > 0.0f ? std::pow (10.0f, (-70.0f + 30.0f * a) / 20.0f) : 0.0f;
        const float drv = 1.0f + 2.0f * a;
        for (int c = 0; c < 2; ++c)
        {
            float x = lp[(size_t) c].process (io[c]);
            x = x + a * (std::tanh (x * drv) / drv - x);         // saturation fades in with age
            x += hissAmp * pinkNoise (c);
            io[c] = x;
        }

        // dropouts: Poisson-timed dips, rate up to ~1.5 / s, depth up to −24 dB
        if (dipLeft > 0) --dipLeft;
        else if (d > 0.0f && rng.pos() < d * 1.5f / (float) sr)
        {
            dipLeft = (int) (sr * (0.03 + 0.17 * rng.pos()));
            dipDepth = std::pow (10.0f, -24.0f * (0.4f + 0.6f * rng.pos()) * d / 20.0f);
        }
        const float dipTarget = dipLeft > 0 ? dipDepth : 1.0f;
        dipGain = dipTarget + dipCoef * (dipGain - dipTarget);

        l = io[0] * dipGain;
        r = io[1] * dipGain;
    }

    /** Fixed transport delay (ms) at rest. */
    static constexpr float kBaseMs = 8.0f;

private:
    float pinkNoise (int c) noexcept
    {
        // Paul Kellet's economy pink filter
        auto& p = pink[(size_t) c];
        const float white = rng.uni() * 0.5f;
        p[0] = 0.99765f * p[0] + white * 0.0990460f;
        p[1] = 0.96300f * p[1] + white * 0.2965164f;
        p[2] = 0.57000f * p[2] + white * 1.0526913f;
        return (p[0] + p[1] + p[2] + white * 0.1848f) * 0.25f;
    }

    double sr = 48000.0, flPhase = 0.0;
    std::array<DelayLine, 2> dl;
    std::array<Biquad, 2> lp;
    std::array<std::array<float, 3>, 2> pink {};
    Smooth wow, flutter, age, drop;
    Rng rng;
    float walk = 0, walkTarget = 0, walkLp = 0.999f;
    int walkTimer = 0, dipLeft = 0, dipTimer = 0;
    float dipGain = 1.0f, dipDepth = 1.0f, dipCoef = 0.99f, lastLpHz = -1.0f;
};

} // namespace vs
