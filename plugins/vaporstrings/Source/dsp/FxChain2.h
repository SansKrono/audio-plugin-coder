#pragma once
/*  VaporStrings — FX chain II (phase 2.1.7). Dependency-free (std only), stereo.

    Haze  pre-delay → reverse-grain swell (300 ms chunks, Hann, 2 readers) →
          4 series allpass diffusers → 8-line FDN (Householder, delays 37–149 ms ×
          size, per-line modulation, one-pole damping, RT60-derived gains).
          Freeze: feedback → 1, damping off, input muted.
    Echo  stereo tape delay (≤ 2.5 s), free ms or host-synced division, smoothed
          time (glides like tape), loop: HPF/LPF tone + tanh limiter + slight wow.

    Both are "send" style with tail spillover: the on/off switch gates the input,
    so tails ring out naturally when an effect is switched off.
      out = dry · (1 − mix·on) + wet · mix                                        */

#include "FxChain1.h"   // DelayLine, Smooth, Rng, Biquad

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vs
{
//==============================================================================
/** Two-reader reverse buffer: each reader plays the previous C samples backwards
    under a Hann window; readers are offset by C/2 so the windows sum to 1. */
class ReverseGrains
{
public:
    void prepare (double sr, float chunkMs)
    {
        chunk = std::max (64, (int) (sr * 0.001 * chunkMs));
        size = 1;
        while (size < 2 * chunk + 4) size <<= 1;
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        reset();
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); t = 0; }

    float process (float x) noexcept
    {
        buf[(size_t) (t & mask)] = x;
        float y = 0.0f;
        const int half = chunk / 2;
        for (int r = 0; r < 2; ++r)
        {
            const int off = r * half;
            const int local = (t + off) % chunk;          // position inside this reader's grain
            const int start = t - local;                  // grain start time
            const int idx = start - 1 - local;            // walk backwards through the previous chunk
            const float w = std::sin (3.14159265f * (float) local / (float) chunk);
            y += buf[(size_t) (idx & mask)] * w * w;
        }
        // wrap at chunk·size: keeps t mod chunk and t mod size intact, never overflows
        if (++t >= chunk * size) t -= chunk * size;
        return y;
    }

private:
    std::vector<float> buf;
    int chunk = 14400, size = 0, mask = 0, t = 0;
};

//==============================================================================
class Haze
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        pre.prepare ((int) (0.52 * sr));
        rev.prepare (sr, 300.0f);
        static constexpr float apMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        for (int k = 0; k < 4; ++k) { ap[(size_t) k].prepare ((int) (apMs[k] * 0.001 * sr) + 2); apLen[(size_t) k] = apMs[k] * 0.001f * (float) sr; }
        for (auto& d : lines) d.prepare ((int) (0.160 * sr));
        size.prepare (sr, 300.0f); decay.prepare (sr, 50.0f); mod.prepare (sr, 50.0f);
        damp.prepare (sr, 50.0f); revAmt.prepare (sr, 30.0f); preMs.prepare (sr, 200.0f);
        inGain.prepare (sr, 15.0f); frz.prepare (sr, 40.0f);
        for (int k = 0; k < N; ++k) lfoInc[(size_t) k] = (0.1 + 1.1 * k / (N - 1.0)) / sr;
        reset();
    }

    void reset()
    {
        pre.reset(); rev.reset();
        for (auto& a : ap) a.reset();
        for (auto& d : lines) d.reset();
        lpZ.fill (0.0f);
        for (int k = 0; k < N; ++k) lfo[(size_t) k] = k / (double) N;
        energy = 0.0f;
        cSz = cDc = cDp = cFz = -1.0f;   // force a coefficient refresh
    }

    struct Params { float size = 0.7f, decayS = 6.0f, predelayMs = 20.0f, mod = 0.4f, damp = 0.4f, reverse = 0.0f; bool freeze = false; bool on = true; };

    void setParams (const Params& p) noexcept
    {
        size.t = std::clamp (p.size, 0.0f, 1.0f);
        decay.t = std::clamp (p.decayS, 0.1f, 60.0f);
        preMs.t = std::clamp (p.predelayMs, 0.0f, 500.0f);
        mod.t = std::clamp (p.mod, 0.0f, 1.0f);
        damp.t = std::clamp (p.damp, 0.0f, 1.0f);
        revAmt.t = std::clamp (p.reverse, 0.0f, 1.0f);
        frz.t = p.freeze ? 1.0f : 0.0f;
        inGain.t = (p.on && ! p.freeze) ? 1.0f : 0.0f;
    }
    void snap() { size.snap (size.t); decay.snap (decay.t); preMs.snap (preMs.t); mod.snap (mod.t); damp.snap (damp.t); revAmt.snap (revAmt.t); frz.snap (frz.t); }

    /** Process one stereo sample; returns the wet signal in wl/wr. */
    void process (float inL, float inR, float& wl, float& wr) noexcept
    {
        const float sz = size.next(), dc = decay.next(), md = mod.next(), dp = damp.next(), rv = revAmt.next();
        const float fz = frz.next(), ig = inGain.next();

        // input: mid, pre-delay, reverse swell
        const float x = 0.5f * (inL + inR) * ig;
        pre.push (x);
        float s = pre.read (std::max (1.0f, preMs.next() * 0.001f * (float) sr));
        const float rs = rev.process (s);
        s = s + rv * (rs - s);

        // diffusion
        for (int k = 0; k < 4; ++k)
        {
            const float d = ap[(size_t) k].read (apLen[(size_t) k]);
            const float v = s + 0.7f * d;
            ap[(size_t) k].push (v);
            s = d - 0.7f * v;
        }

        // FDN read (modulated)
        static constexpr float baseMs[N] = { 37.0f, 47.0f, 59.0f, 71.0f, 89.0f, 103.0f, 127.0f, 149.0f };
        std::array<float, N> y {};
        float sum = 0.0f;

        // Loop coefficients depend only on size / decay / damping / freeze: recompute
        // them when one of those smoothed controls moves, not on every sample.
        //   damping: one-pole LP from ~18 kHz (0) to ~2 kHz (1). Freeze bypasses it
        //   completely and trims modulation to 25 % (linear-interpolated modulated reads
        //   low-pass slightly on every pass, which would otherwise drain the frozen tail).
        if (sz != cSz || dc != cDc || dp != cDp || fz != cFz)
        {
            const float dampHz = 18000.0f * std::pow (2000.0f / 18000.0f, dp);
            dampA = std::exp (-2.0f * 3.14159265f * dampHz / (float) sr) * (1.0f - fz);
            const float scale = 0.35f + 0.65f * sz;
            for (int k = 0; k < N; ++k)
            {
                lineLen[(size_t) k] = baseMs[k] * scale * 0.001f * (float) sr;
                const float g = std::pow (10.0f, -3.0f * baseMs[k] * scale * 0.001f / dc);   // RT60 gain
                lineGain[(size_t) k] = g + fz * (1.0f - g);                                     // frozen → unity
            }
            cSz = sz; cDc = dc; cDp = dp; cFz = fz;
        }
        const float modSamp = md * (1.0f - 0.75f * fz) * 0.001f * (float) sr;
        for (int k = 0; k < N; ++k)
        {
            lfo[(size_t) k] += lfoInc[(size_t) k];
            if (lfo[(size_t) k] >= 1.0) lfo[(size_t) k] -= 1.0;
            const float dSamp = lineLen[(size_t) k] + modSamp * fastSin2pi ((float) lfo[(size_t) k]);
            float v = lines[(size_t) k].read (std::max (2.0f, dSamp));
            v = v + dampA * (lpZ[(size_t) k] - v);
            lpZ[(size_t) k] = v;
            y[(size_t) k] = v * lineGain[(size_t) k];
            sum += y[(size_t) k];
        }

        // Householder feedback + input injection (alternating signs), write back
        const float h = (2.0f / N) * sum;
        float l = 0.0f, r = 0.0f, e = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            const float fb = y[(size_t) k] - h;
            lines[(size_t) k].push (fb + ((k & 1) ? -s : s) * 0.5f);
            l += ((k & 2) ? -y[(size_t) k] : y[(size_t) k]);
            r += ((k & 4) ? -y[(size_t) k] : y[(size_t) k]) * ((k & 1) ? -1.0f : 1.0f);
            e += y[(size_t) k] * y[(size_t) k];
        }
        energy = e;
        wl = l * 0.35f;
        wr = r * 0.35f;
    }

    /** True while the tail is audible (used to skip a fully-decayed, switched-off Haze). */
    bool isRinging() const noexcept { return energy > 1.0e-9f; }

private:
    double sr = 48000.0;
    DelayLine pre;
    ReverseGrains rev;
    std::array<DelayLine, 4> ap;
    std::array<float, 4> apLen {};
    std::array<DelayLine, N> lines;
    std::array<float, N> lpZ {};
    std::array<double, N> lfo {}, lfoInc {};
    Smooth size, decay, mod, damp, revAmt, preMs, inGain, frz;
    float energy = 0.0f;
    // cached loop coefficients (see process)
    std::array<float, N> lineLen {}, lineGain {};
    float dampA = 0.0f, cSz = -1.0f, cDc = -1.0f, cDp = -1.0f, cFz = -1.0f;
};

//==============================================================================
class Echo
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : dl) d.prepare ((int) (2.6 * sr));
        timeS.prepare (sr, 120.0f); fbS.prepare (sr, 30.0f); inGain.prepare (sr, 15.0f);
        reset();
    }
    void reset()
    {
        for (auto& d : dl) d.reset();
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        wowPh = 0.0; energy = 0.0f; lastTone = -1.0f;
    }

    /** Beats per division, matching params.json echo_div order. */
    static float divisionBeats (int idx) noexcept
    {
        static constexpr float beats[] = { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
        return beats[std::clamp (idx, 0, 11)];
    }

    struct Params { bool sync = true; float timeMs = 375.0f; int division = 6; double bpm = 120.0; float feedback = 0.35f; float tone = 0.5f; bool on = true; };

    void setParams (const Params& p) noexcept
    {
        const double ms = p.sync ? 60000.0 / std::clamp (p.bpm, 20.0, 400.0) * divisionBeats (p.division) : p.timeMs;
        timeS.t = (float) std::clamp (ms, 10.0, 2500.0) * 0.001f * (float) sr;
        fbS.t = std::clamp (p.feedback, 0.0f, 0.95f);
        inGain.t = p.on ? 1.0f : 0.0f;
        if (std::abs (p.tone - lastTone) > 1.0e-4f)
        {
            const float t = std::clamp (p.tone, 0.0f, 1.0f);
            for (auto& f : lp) f.setLowPass (sr, 1500.0 * std::pow (8.0, t), 0.7071);   // 1.5 → 12 kHz
            for (auto& f : hp) f.setHighPass (sr, 80.0 * std::pow (5.0, t), 0.7071);    // 80 → 400 Hz
            lastTone = p.tone;
        }
    }
    void snap() { timeS.snap (timeS.t); fbS.snap (fbS.t); inGain.snap (inGain.t); }

    void process (float inL, float inR, float& wl, float& wr) noexcept
    {
        const float d = timeS.next(), fb = fbS.next(), ig = inGain.next();
        wowPh += 0.4 / sr;
        if (wowPh >= 1.0) wowPh -= 1.0;
        const float wob = 0.0004f * (float) sr * fastSin2pi ((float) wowPh);   // ±0.4 ms
        const float in[2] = { inL * ig, inR * ig };
        float out[2];
        float e = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            const float tap = dl[(size_t) c].read (std::max (1.0f, d + wob));
            float loop = hp[(size_t) c].process (lp[(size_t) c].process (tap));
            loop = std::tanh (loop * fb * 1.1f) / 1.1f;                                   // limiter in the loop
            dl[(size_t) c].push (in[c] + loop);
            out[c] = tap;
            e += tap * tap;
        }
        energy = e;
        wl = out[0];
        wr = out[1];
    }

    bool isRinging() const noexcept { return energy > 1.0e-10f; }

private:
    double sr = 48000.0, wowPh = 0.0;
    std::array<DelayLine, 2> dl;
    std::array<Biquad, 2> lp, hp;
    Smooth timeS, fbS, inGain;
    float energy = 0.0f, lastTone = -1.0f;
};

//==============================================================================
/** Runs a send-style FX (Haze / Echo) with spillover over a stereo block.
    `active` keeps it processing while switched on or still ringing. */
template <class Fx>
inline void runSend (Fx& fx, bool on, float mixTarget, Smooth& mix, Smooth& onS, bool& active,
                     float* l, float* r, int n) noexcept
{
    if (on && ! active) { fx.reset(); fx.snap(); active = true; }
    if (! active) return;
    mix.t = mixTarget;
    onS.t = on ? 1.0f : 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float wl, wr;
        fx.process (l[i], r[i], wl, wr);
        const float m = mix.next(), o = onS.next();
        l[i] = l[i] * (1.0f - m * o) + wl * m;
        r[i] = r[i] * (1.0f - m * o) + wr * m;
    }
    if (! on && ! fx.isRinging() && onS.v < 1.0e-4f)
        active = false;
}

} // namespace vs
