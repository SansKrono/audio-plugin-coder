#pragma once
/*  VaporStrings — synth source [C] (phase 2.1.3)
    Monophonic, driven by the pitch tracker. Dependency-free (std only).

    f0 → glide (one-pole in log2-frequency) → Osc 1 / Osc 2 (PolyBLEP saw, square,
    pulse, triangle, sine) × unison 1–7 (detune ±spread·25 ct, panned) + sine sub
    one octave under Osc 1. Free-running phases (no resets → no clicks).          */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace vs
{
class SynthSource
{
public:
    enum class Wave { Saw = 0, Square, Pulse, Triangle, Sine };
    static constexpr int kMaxUnison = 7;

    struct Params
    {
        Wave wave1 = Wave::Saw, wave2 = Wave::Square;
        int oct1 = 0, oct2 = 0;
        float detune2Cents = 7.0f;
        float oscMix = 0.5f;       // 0 = Osc 1, 1 = Osc 2
        float pulseWidth = 0.5f;
        int unison = 1;
        float spread = 0.3f;       // 0..1 → ±25 ct, full stereo pan
        float subLevel = 0.0f;
        float glideMs = 30.0f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        uint32_t s = 0x9E3779B9u;
        for (auto& o : osc)
            for (auto& v : o)
            {
                s = s * 1664525u + 1013904223u;
                v.phase = (float) (s >> 8) / 16777216.0f;   // spread start phases
                v.tri = 0.0f;
            }
        subPhase = 0.0f;
        logHz = -1.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        params.unison = std::clamp (p.unison, 1, kMaxUnison);
        params.pulseWidth = std::clamp (p.pulseWidth, 0.05f, 0.95f);

        const int n = params.unison;
        const float r1 = std::exp2 ((float) params.oct1);
        const float r2 = std::exp2 ((float) params.oct2 + params.detune2Cents / 1200.0f);
        for (int k = 0; k < n; ++k)
        {
            const float pos = n > 1 ? (2.0f * (float) k / (float) (n - 1) - 1.0f) : 0.0f;   // -1..1
            const float cents = pos * params.spread * 25.0f;
            const float d = std::exp2 (cents / 1200.0f);
            ratio[0][(size_t) k] = r1 * d;
            ratio[1][(size_t) k] = r2 / d;                    // mirror osc 2 for width
            const float pan = pos * params.spread;            // -1..1
            const float a = (pan + 1.0f) * 0.25f * 3.14159265f;
            panL[(size_t) k] = std::cos (a);
            panR[(size_t) k] = std::sin (a);
        }
        unisonGain = 1.0f / std::sqrt ((float) n);
        subRatio = r1 * 0.5f;

        glideCoef = params.glideMs <= 0.5f ? 0.0f : std::exp (-1.0f / (0.001f * params.glideMs * (float) sr));
    }

    /** LFO → PWM offset, applied on top of the pulse-width parameter (control rate). */
    void setPulseWidthOffset (float off) noexcept { pwOffset = off; }

    /** One stereo sample. hz = tracked pitch (0 = nothing yet). amp = 0..1 gain. */
    void tick (float hz, float amp, float& outL, float& outR) noexcept
    {
        outL = outR = 0.0f;
        if (hz <= 0.0f)
            return;

        const float target = std::log2 (hz);
        if (logHz < 0.0f) logHz = target;                                // first note: no slide-in
        logHz = target + glideCoef * (logHz - target);
        const float base = std::exp2 (logHz);

        if (amp <= 1.0e-6f)
        {
            advanceSilently (base);
            return;
        }

        const float m2 = params.oscMix, m1 = 1.0f - m2;
        const float nyq = 0.45f * (float) sr;
        float l = 0.0f, r = 0.0f;

        for (int k = 0; k < params.unison; ++k)
        {
            const float s1 = m1 > 0.0f ? render (osc[0][(size_t) k], params.wave1, std::min (base * ratio[0][(size_t) k], nyq)) : 0.0f;
            const float s2 = m2 > 0.0f ? render (osc[1][(size_t) k], params.wave2, std::min (base * ratio[1][(size_t) k], nyq)) : 0.0f;
            const float v = m1 * s1 + m2 * s2;
            l += v * panL[(size_t) k];
            r += v * panR[(size_t) k];
        }
        l *= unisonGain;
        r *= unisonGain;

        // sine sub, one octave under Osc 1 (centre)
        if (params.subLevel > 0.0f)
        {
            const float sub = std::sin (6.2831853f * subPhase) * params.subLevel * 0.7071f;
            l += sub;
            r += sub;
        }
        subPhase += std::min (base * subRatio, nyq) / (float) sr;
        subPhase -= std::floor (subPhase);

        outL = l * amp * 0.5f;   // -6 dB headroom: a full saw stack is hot next to a DI
        outR = r * amp * 0.5f;
    }

    /** Current (glided) base pitch in Hz; 0 before the first note. */
    float getCurrentHz() const noexcept { return logHz < 0.0f ? 0.0f : std::exp2 (logHz); }

private:
    struct Voice { float phase = 0.0f, tri = 0.0f; };

    static float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)        { t /= dt;               return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float render (Voice& v, Wave w, float freq) noexcept
    {
        const float dt = freq / (float) sr;
        const float p = v.phase;
        float y = 0.0f;

        switch (w)
        {
            case Wave::Saw:
                y = 2.0f * p - 1.0f - polyBlep (p, dt);
                break;

            case Wave::Square:
            case Wave::Pulse:
            case Wave::Triangle:
            {
                const float pw = w == Wave::Pulse ? std::clamp (params.pulseWidth + pwOffset, 0.05f, 0.95f) : 0.5f;
                float q = p < pw ? 1.0f : -1.0f;
                float p2 = p + (1.0f - pw);
                p2 -= std::floor (p2);
                q += polyBlep (p, dt) - polyBlep (p2, dt);
                if (w == Wave::Triangle)
                {
                    // leaky integration of a band-limited square → band-limited triangle
                    v.tri = 0.9995f * v.tri + 4.0f * dt * q;
                    y = v.tri;
                }
                else
                {
                    y = q - (w == Wave::Pulse ? (2.0f * pw - 1.0f) : 0.0f);   // remove pulse DC
                }
                break;
            }

            case Wave::Sine:
                y = std::sin (6.2831853f * p);
                break;
        }

        v.phase += dt;
        v.phase -= std::floor (v.phase);
        return y;
    }

    void advanceSilently (float base) noexcept
    {
        // keep phases running so notes re-enter mid-cycle (no synchronous clicks)
        for (int o = 0; o < 2; ++o)
            for (int k = 0; k < params.unison; ++k)
            {
                auto& v = osc[(size_t) o][(size_t) k];
                v.phase += base * ratio[(size_t) o][(size_t) k] / (float) sr;
                v.phase -= std::floor (v.phase);
                v.tri *= 0.999f;
            }
        subPhase += base * subRatio / (float) sr;
        subPhase -= std::floor (subPhase);
    }

    double sr = 48000.0;
    Params params;
    std::array<std::array<Voice, kMaxUnison>, 2> osc {};
    std::array<std::array<float, kMaxUnison>, 2> ratio {};
    std::array<float, kMaxUnison> panL {}, panR {};
    float unisonGain = 1.0f, subRatio = 0.5f, subPhase = 0.0f;
    float logHz = -1.0f, glideCoef = 0.0f, pwOffset = 0.0f;
};

} // namespace vs
