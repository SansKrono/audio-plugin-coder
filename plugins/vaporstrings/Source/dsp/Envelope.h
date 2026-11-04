#pragma once
/*  VaporStrings — voice envelope (phase 2.1.5). Dependency-free (std only).

    One envelope drives both the amp of source C and the filter env amount.
      Follow  the guitar's own envelope, normalised to the pick peak (1.0 at the
              attack, decaying with the string) × velocity.
      ADSR    triggered by the pick-onset detector, released when the noise gate
              closes. Linear attack, exponential decay/release. × velocity.
      Swell   the Follow shape × a re-triggered 0→1 attack ramp (env_attack), for
              string-machine pads that bloom into every note.

    level()  → synth amp + filter env (includes velocity)
    shape()  → gain for sources A/B: 1 in Follow (natural guitar dynamics), the
               ADSR / swell contour otherwise (no velocity — the guitar has its own). */

#include <algorithm>
#include <cmath>

namespace vs
{
class VoiceEnvelope
{
public:
    enum class Mode { Follow = 0, ADSR, Swell };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        // pick-peak memory: holds for the whole note (each pick resets it); the slow
        // 10 s leak only matters when no pick is ever detected (e.g. volume swells)
        peakDecay = std::exp (-1.0f / (10.0f * sr));
        reset();
    }

    void reset()
    {
        adsr = 0.0f; stage = Stage::Idle;
        ramp = 0.0f; pickPeak = 1.0e-4f;
        velocity = 1.0f; out = 0.0f; contour = 1.0f;
    }

    void setParams (Mode m, float attackMs, float decayMs, float sustain, float releaseMs) noexcept
    {
        mode = m;
        attackStep = 1.0f / std::max (1.0f, 0.001f * attackMs * sr);
        // exponential stages reach ~-60 dB (≈ 1/1000) in the given time
        decayCoef   = std::exp (-6.9f / std::max (1.0f, 0.001f * decayMs * sr));
        releaseCoef = std::exp (-6.9f / std::max (1.0f, 0.001f * releaseMs * sr));
        sustainLvl  = std::clamp (sustain, 0.0f, 1.0f);
    }

    /** A pick was detected. velocity 0..1 (already scaled by vel_sens). */
    void noteOn (float vel, float followerNow) noexcept
    {
        velocity = std::clamp (vel, 0.0f, 1.0f);
        stage = Stage::Attack;                 // legato: attack rises from the current level
        ramp = 0.0f;
        pickPeak = std::max (followerNow, 1.0e-4f);
    }

    /** Per sample. follower = guitar envelope follower, gateOpen = noise gate state. */
    void tick (float follower, bool gateOpen) noexcept
    {
        // pick-peak normaliser for Follow / Swell
        pickPeak = std::max (follower, pickPeak * peakDecay);
        // (the follower already decays to silence once the gate has closed)
        const float follow = follower < 1.0e-5f ? 0.0f : std::min (1.0f, follower / std::max (pickPeak, 1.0e-4f));

        // ADSR
        if (! gateOpen && stage != Stage::Idle && stage != Stage::Release)
            stage = Stage::Release;
        switch (stage)
        {
            case Stage::Attack:
                adsr += attackStep;
                if (adsr >= 1.0f) { adsr = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                adsr = sustainLvl + decayCoef * (adsr - sustainLvl);
                if (adsr - sustainLvl < 1.0e-4f) { adsr = sustainLvl; stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                adsr = sustainLvl;
                break;
            case Stage::Release:
                adsr *= releaseCoef;
                if (adsr < 1.0e-5f) { adsr = 0.0f; stage = Stage::Idle; }
                break;
            case Stage::Idle:
                break;
        }

        // Swell ramp (smoothstep so the bloom has no corner)
        ramp = std::min (1.0f, ramp + attackStep);
        const float bloom = ramp * ramp * (3.0f - 2.0f * ramp);

        switch (mode)
        {
            case Mode::Follow: contour = 1.0f;  out = follow * velocity;         break;
            case Mode::ADSR:   contour = adsr;  out = adsr * velocity;           break;
            case Mode::Swell:  contour = bloom; out = bloom * follow * velocity; break;
        }
    }

    float level() const noexcept { return out; }
    float shape() const noexcept { return contour; }
    bool  isActive() const noexcept { return stage != Stage::Idle; }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    float sr = 48000.0f;
    Mode mode = Mode::Follow;
    Stage stage = Stage::Idle;
    float attackStep = 0.01f, decayCoef = 0.999f, releaseCoef = 0.999f, sustainLvl = 0.8f;
    float adsr = 0.0f, ramp = 0.0f, pickPeak = 1.0e-4f, peakDecay = 0.9999f;
    float velocity = 1.0f, out = 0.0f, contour = 1.0f;
};

} // namespace vs
