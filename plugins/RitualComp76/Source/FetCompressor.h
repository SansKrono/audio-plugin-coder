#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ParameterIDs.hpp"

/**
    FET gain cell + feedback detector + gain computer.

    Feedback topology: the detector senses the gain cell's OUTPUT level
    (input level minus the current gain reduction), like the original hardware.
    For a feedback loop, the gain reduction that yields an input/output ratio R is

        GR = f(over) = (R - 1) * over(L_out - threshold)

    The loop gain is 1 + f'(over) * slope(knee), which would speed up the
    ballistics by that factor and can go unstable at high ratios with
    microsecond attacks. Each per-sample coefficient is divided by the current
    loop gain, so the closed-loop time constants match the Attack/Release knobs
    and the loop stays stable at any ratio and sample rate.

    Phase 4.1.2 character:
      - Program-dependent release: blends fast (knob) and slow (4x) release
        stages by how much gain reduction has been sustained recently.
      - FET distortion: GR-dependent, mainly 2nd-harmonic shaping in the cell.
      - All-buttons-in: level-dependent ratio (12:1 near threshold, climbing
        past 20:1), GR overshoot on attack, slower attack / faster release,
        3x FET distortion.

    Stereo: linked detector (max of channel peaks), one GR applied to all channels.
    Produces DC when distorting: follow with a DC blocker.
*/
class FetCompressor
{
public:
    struct Settings
    {
        float attackStep  = ParameterRanges::attackDefault;   // 1 (slow) .. 7 (fast)
        float releaseStep = ParameterRanges::releaseDefault;  // 1 (slow) .. 7 (fast)
        RatioMode ratio   = RatioMode::R4;
    };

    void prepare (double sampleRate);
    void reset();

    /** Call once per block (allocation-free). */
    void setSettings (const Settings& s);

    /** Processes the gain cell in place. Returns the maximum gain reduction (dB, >= 0) in the block. */
    float process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Gain reduction actually applied (dB, >= 0), including the All-mode bump. */
    float getCurrentGainReductionDb() const noexcept { return appliedGrDb; }
    bool isAllMode() const noexcept { return targetAllness > 0.5f; }

    /** Jump curve parameters to their targets (no glide). Called by prepare(). */
    void snapToTargets() noexcept;

    // Knob-step -> time mappings (exponential, matching the UI readouts)
    static float attackSecondsFromStep (float step) noexcept;   // 800 µs .. 20 µs
    static float releaseSecondsFromStep (float step) noexcept;  // 1100 ms .. 50 ms

    struct RatioCurve { float ratio, thresholdDb, kneeDb; bool active; };
    static RatioCurve curveFor (RatioMode mode) noexcept;

    // All-buttons-in model constants (exposed for tests)
    static constexpr float allRatioSlope     = 0.8f;   // ratio grows per dB over threshold
    static constexpr float allRatioMax       = 40.0f;
    static constexpr float allOvershoot      = 0.5f;   // extra GR per dB that GR sits above its 40 ms average
    static constexpr float allAttackScale    = 2.0f;   // attack time x2
    static constexpr float allReleaseScale   = 0.6f;   // release time x0.6
    static constexpr float slowReleaseFactor = 4.0f;   // slow release stage = 4x knob time
    static constexpr float sustainFullDb     = 12.0f;  // sustained GR for fully-slow release

private:
    double sampleRate = 44100.0;
    float grDb = 0.0f;              // current gain reduction, dB (>= 0)
    float sustainDb = 0.0f;         // slow average of GR (program dependence)

    float attackCoeff = 1.0f;       // open-loop one-pole coefficients
    float releaseFastCoeff = 1.0f;
    float releaseSlowCoeff = 1.0f;
    float sustainCoeff = 0.0f;
    float allBumpCoeff = 0.0f;
    float grSlow = 0.0f;            // 40 ms average of GR (All-mode bump detector)
    float appliedGrDb = 0.0f;       // GR actually applied (loop GR + All-mode bump)

    // Curve parameters: targets set per block, current values glide per sample (click-free switching)
    bool  active = true;
    float targetRatio = 4.0f, targetThresholdDb = -10.0f, targetKneeDb = 6.0f;
    float targetAllness = 0.0f, targetFetAmount = 0.0f;
    float ratio = 4.0f, thresholdDb = -10.0f, kneeDb = 6.0f;
    float allness = 0.0f;           // 0 = normal ratios, 1 = all-buttons-in behaviour
    float fetAmount = 0.0f;         // 2nd-harmonic amount per 10 dB GR
    float glideCoeff = 1.0f;
};
