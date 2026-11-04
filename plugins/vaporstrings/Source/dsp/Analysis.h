#pragma once
/*  VaporStrings — analysis engine (phase 2.1.2)
    Dependency-free (std only) so tests/TrackerTest.cpp can exercise it offline.

    PitchTracker   tracking HPF + fixed 1.2 kHz LPF → ÷D decimation (~12 kHz) →
                   YIN (W = 256, hop = 64) → octave guard → median → hold.
    OnsetDetector  fast/slow envelope ratio on the HPF'd signal → noteOn + velocity.

    All buffers are sized in prepare(); process*() never allocates.            */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vs
{
//==============================================================================
/** RBJ biquad, transposed direct form II. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setHighPass (double sr, double f, double q = 0.70710678) noexcept { set (sr, f, q, true); }
    void setLowPass  (double sr, double f, double q = 0.70710678) noexcept { set (sr, f, q, false); }

private:
    void set (double sr, double f, double q, bool hp) noexcept
    {
        f = std::clamp (f, 5.0, sr * 0.45);
        const double w = 2.0 * 3.14159265358979323846 * f / sr;
        const double cs = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        const double n0 = hp ? (1.0 + cs) * 0.5 : (1.0 - cs) * 0.5;
        const double n1 = hp ? -(1.0 + cs) : (1.0 - cs);
        b0 = (float) (n0 / a0);
        b1 = (float) (n1 / a0);
        b2 = (float) (n0 / a0);
        a1 = (float) (-2.0 * cs / a0);
        a2 = (float) ((1.0 - al) / a0);
    }
};

//==============================================================================
class PitchTracker
{
public:
    enum class Priority { Last = 0, Lowest = 1, Highest = 2 };

    static constexpr int kWindow = 256;     // YIN integration window (decimated samples)
    static constexpr int kHop    = 64;      // analysis hop (~5 ms at 12 kHz)
    static constexpr float kMinHz = 60.0f;  // below low E (82 Hz) incl. drop tunings
    static constexpr float kMaxHz = 1400.0f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::lround (sampleRate / 12000.0));
        dsr = sampleRate / decim;

        tauMin = std::max (2, (int) std::floor (dsr / kMaxHz));
        tauMax = (int) std::ceil (dsr / kMinHz);
        bufLen = kWindow + tauMax + 2;

        ring.assign ((size_t) bufLen * 2, 0.0f);   // mirrored ring → contiguous reads
        diff.assign ((size_t) tauMax + 2, 0.0f);
        cmnd.assign ((size_t) tauMax + 2, 1.0f);

        lp1.setLowPass (sr, 1200.0, 0.5412);       // 4th-order Butterworth @ 1.2 kHz
        lp2.setLowPass (sr, 1200.0, 1.3066);       // (also the anti-alias for ÷D)
        setHighPassHz (70.0f);
        reset();
    }

    void reset()
    {
        hp.reset(); lp1.reset(); lp2.reset();
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = 0; filled = 0; decimCount = 0; hopCount = 0;
        medianLen = 0; medianFill = 0; medianPos = 0;
        heldHz = 0.0f; voiced = false; confidence = 0.0f;
        pendingHz = 0.0f; pendingCount = 0;
    }

    void setHighPassHz (float hz) { if (sr > 0) hp.setHighPass (sr, hz); }

    /** speed 0..1 (stable ↔ fast), sens 0..1 (strict ↔ lenient) */
    void setResponse (float speed, float sens, Priority p) noexcept
    {
        threshold = 0.05f + 0.30f * std::clamp (sens, 0.0f, 1.0f);
        const int len = 1 + 2 * (int) std::lround ((1.0f - std::clamp (speed, 0.0f, 1.0f)) * 3.0f); // 1,3,5,7
        if (len != medianLen) { medianLen = len; medianFill = 0; medianPos = 0; }
        priority = p;
    }

    /** Feed one input sample (full rate). gateOpen = note gate from the noise gate. */
    void push (float x, bool gateOpen) noexcept
    {
        const float y = lp2.process (lp1.process (hp.process (x)));
        if (++decimCount < decim)
            return;
        decimCount = 0;

        ring[(size_t) writePos] = y;
        ring[(size_t) (writePos + bufLen)] = y;
        writePos = (writePos + 1) % bufLen;
        if (filled < bufLen) ++filled;

        if (++hopCount >= kHop)
        {
            hopCount = 0;
            if (filled >= bufLen)
                analyse (gateOpen);
        }
    }

    /** Last stable pitch (held through unvoiced frames). 0 until the first note. */
    float getHz() const noexcept         { return heldHz; }
    bool  isVoiced() const noexcept      { return voiced; }
    float getConfidence() const noexcept { return confidence; }

private:
    void analyse (bool gateOpen) noexcept
    {
        // oldest sample of the analysis span starts at writePos (mirrored ring)
        const float* x = ring.data() + writePos;

        // energy gate: skip silence (also stops YIN locking onto noise floor)
        float energy = 0.0f;
        for (int j = 0; j < kWindow; ++j) energy += x[j] * x[j];
        if (! gateOpen || energy < 1.0e-7f)
        {
            voiced = false;
            confidence *= 0.6f;
            return;
        }

        // YIN difference function + cumulative mean normalisation
        cmnd[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            const float* xt = x + tau;
            for (int j = 0; j < kWindow; ++j)
            {
                const float df = x[j] - xt[j];
                d += df * df;
            }
            diff[(size_t) tau] = d;
            running += d;
            cmnd[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }

        // candidate selection
        int best = -1;
        if (priority == Priority::Lowest)
        {
            // deepest dip overall (robust on wound strings / low notes)
            float m = 1.0e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cmnd[(size_t) tau] < m && isLocalMin (tau)) { m = cmnd[(size_t) tau]; best = tau; }
            if (best >= 0 && m > threshold) best = -1;
        }
        else
        {
            // standard YIN: first dip under threshold, walked to its local minimum.
            // Highest: stricter threshold first, which favours shorter periods.
            const float th = priority == Priority::Highest ? threshold * 0.6f : threshold;
            best = firstBelow (th);
            if (best < 0 && priority == Priority::Highest)
                best = firstBelow (threshold);
        }

        // octave-up guard: when the 2nd harmonic dominates, the half-period dip can
        // clear the threshold first. If the dip at 2τ is far deeper, 2τ is the period.
        // Only a *weak* match (cmnd > 0.08) is questioned: a clean period dip is near
        // zero at both τ and 2τ, and ratios between tiny values are meaningless.
        if (best > 0 && priority != Priority::Highest && cmnd[(size_t) best] > 0.08f)
        {
            int dbl = -1;
            float dv = 1.0e9f;
            for (int t = 2 * best - 2; t <= 2 * best + 2; ++t)
                if (t > tauMin && t < tauMax && cmnd[(size_t) t] < dv) { dv = cmnd[(size_t) t]; dbl = t; }
            if (dbl > 0 && dv < 0.5f * cmnd[(size_t) best])
                best = dbl;
        }

        if (best < 0)
        {
            voiced = false;
            confidence = std::max (0.0f, 1.0f - *std::min_element (cmnd.begin() + tauMin, cmnd.begin() + tauMax));
            return;
        }

        // parabolic interpolation on the normalised difference
        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = cmnd[(size_t) best - 1], s1 = cmnd[(size_t) best], s2 = cmnd[(size_t) best + 1];
            const float den = s0 - 2.0f * s1 + s2;
            if (std::abs (den) > 1.0e-9f)
                tauF += 0.5f * (s0 - s2) / den;
        }

        const float hz = (float) (dsr / tauF);
        confidence = std::clamp (1.0f - cmnd[(size_t) best], 0.0f, 1.0f);

        // octave guard: an exact ×2 / ×0.5 jump must persist for 2 frames
        if (heldHz > 0.0f)
        {
            const float r = hz / heldHz;
            const bool octaveJump = std::abs (r - 2.0f) < 0.06f || std::abs (r - 0.5f) < 0.015f;
            if (octaveJump)
            {
                if (pendingCount > 0 && std::abs (hz / pendingHz - 1.0f) < 0.03f) ++pendingCount;
                else { pendingHz = hz; pendingCount = 1; }
                if (pendingCount < 2) { voiced = true; return; }
            }
            pendingCount = 0;
        }

        heldHz = medianPush (hz);
        voiced = true;
    }

    bool isLocalMin (int tau) const noexcept
    {
        return cmnd[(size_t) tau] <= cmnd[(size_t) tau - 1] && cmnd[(size_t) tau] <= cmnd[(size_t) tau + 1];
    }

    int firstBelow (float th) const noexcept
    {
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < th)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                return tau;
            }
        }
        return -1;
    }

    float medianPush (float hz) noexcept
    {
        if (medianLen <= 1) return hz;
        median[(size_t) medianPos] = hz;
        medianPos = (medianPos + 1) % medianLen;
        medianFill = std::min (medianFill + 1, medianLen);
        std::array<float, 7> tmp {};
        std::copy (median.begin(), median.begin() + medianFill, tmp.begin());
        std::nth_element (tmp.begin(), tmp.begin() + medianFill / 2, tmp.begin() + medianFill);
        return tmp[(size_t) medianFill / 2];
    }

    double sr = 0.0, dsr = 0.0;
    int decim = 1, decimCount = 0, hopCount = 0;
    int tauMin = 2, tauMax = 200, bufLen = 0, writePos = 0, filled = 0;
    std::vector<float> ring, diff, cmnd;
    Biquad hp, lp1, lp2;

    float threshold = 0.2f;
    Priority priority = Priority::Last;
    std::array<float, 7> median {};
    int medianLen = 0, medianFill = 0, medianPos = 0;

    float heldHz = 0.0f, confidence = 0.0f, pendingHz = 0.0f;
    int pendingCount = 0;
    bool voiced = false;
};

//==============================================================================
/** Pick-attack detector ("level rise"): a peak follower (0.5 ms attack, 60 ms
    release) is compared with its own value 20 ms earlier. A held or decaying
    note keeps that ratio ≤ ~1.2 even at 82 Hz (release ripple); a pick makes
    it leap. Retriggers are locked out for 40 ms. */
class OnsetDetector
{
public:
    static constexpr int kHistory = 32;   // 1 ms slots
    static constexpr int kLookback = 20;  // ms

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hp.setHighPass (sr, 120.0);
        fastA = coef (0.5f);
        fastR = coef (60.0f);
        slotLen = std::max (1, (int) std::lround (sr / 1000.0));
        lockoutLen = (int) (0.040 * sr);
        velWindowLen = (int) (0.005 * sr);
        reset();
    }

    void reset()
    {
        hp.reset();
        fast = 0.0f;
        history.fill (0.0f);
        histPos = 0; slotCount = 0;
        lockout = velWindow = 0;
        velPeak = 0.0f; velocity = 0.0f;
        onsetCount = 0;
    }

    /** sens 0..1: higher = triggers on softer picking. velSens 0..1. */
    void setSensitivity (float sens, float velSensIn) noexcept
    {
        ratio = 2.2f - 1.2f * std::clamp (sens, 0.0f, 1.0f);   // 2.2 … 1.0 (+ floor below)
        velSens = std::clamp (velSensIn, 0.0f, 1.0f);
    }

    /** Returns true on the sample where a new onset's velocity is final. */
    bool push (float x, float floorLevel) noexcept
    {
        const float a = std::abs (hp.process (x));
        fast = a > fast ? fastA * (fast - a) + a : fastR * (fast - a) + a;

        if (++slotCount >= slotLen)
        {
            slotCount = 0;
            history[(size_t) histPos] = fast;
            histPos = (histPos + 1) % kHistory;
        }
        const float past = history[(size_t) ((histPos - kLookback + kHistory) % kHistory)];

        if (lockout > 0) --lockout;

        if (velWindow > 0)
        {
            velPeak = std::max (velPeak, fast);
            if (--velWindow == 0)
            {
                // map peak (≈ -40 … -6 dBFS) to 0..1, then blend toward 1 as vel_sens falls
                const float db = 20.0f * std::log10 (std::max (velPeak, 1.0e-6f));
                const float raw = std::clamp ((db + 40.0f) / 34.0f, 0.0f, 1.0f);
                velocity = 1.0f - velSens * (1.0f - raw);
                ++onsetCount;
                return true;
            }
            return false;
        }

        if (lockout == 0 && fast > floorLevel && fast > past * ratio + 1.0e-4f)
        {
            lockout = lockoutLen;
            velWindow = std::max (1, velWindowLen);
            velPeak = fast;
        }
        return false;
    }

    float getVelocity() const noexcept      { return velocity; }
    uint32_t getOnsetCount() const noexcept { return onsetCount; }

private:
    float coef (float ms) const { return (float) std::exp (-1.0 / (0.001 * ms * sr)); }

    double sr = 44100.0;
    Biquad hp;
    float fast = 0, fastA = 0, fastR = 0;
    std::array<float, kHistory> history {};
    int histPos = 0, slotCount = 0, slotLen = 48;
    float ratio = 1.6f, velSens = 0.5f, velPeak = 0, velocity = 0;
    int lockout = 0, lockoutLen = 0, velWindow = 0, velWindowLen = 0;
    uint32_t onsetCount = 0;
};

} // namespace vs
