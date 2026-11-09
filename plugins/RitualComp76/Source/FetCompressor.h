#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ParameterIDs.hpp"

/**
    FET gain cell + feedback detector + gain computer (Phase 4.1.1 core).

    Feedback topology: the detector senses the gain cell's OUTPUT level
    (input level minus the current gain reduction), like the original hardware.
    For a feedback loop, the gain reduction that yields an input/output ratio R is

        GR = (R - 1) * over(L_out - threshold)

    The loop gain is 1 + (R - 1) * slope(knee), which would speed up the
    ballistics by that factor and can go unstable at high ratios with
    microsecond attacks. Each per-sample coefficient is divided by the current
    loop gain, so the closed-loop time constants match the Attack/Release knobs
    exactly and the loop stays stable at any ratio and sample rate.

    Stereo: linked detector (max of channel peaks), one GR applied to all channels.
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

    /** Call once per block (not real-time critical, but allocation-free). */
    void setSettings (const Settings& s);

    /** Processes the gain cell in place. Returns the maximum gain reduction (dB, >= 0) in the block. */
    float process (float* const* channels, int numChannels, int numSamples) noexcept;

    float getCurrentGainReductionDb() const noexcept { return grDb; }

    // Knob-step -> time mappings (exponential, matching the UI readouts)
    static float attackSecondsFromStep (float step) noexcept;   // 800 µs .. 20 µs
    static float releaseSecondsFromStep (float step) noexcept;  // 1100 ms .. 50 ms

    struct RatioCurve { float ratio, thresholdDb, kneeDb; bool active; };
    static RatioCurve curveFor (RatioMode mode) noexcept;

private:
    double sampleRate = 44100.0;
    float grDb = 0.0f;              // current gain reduction, dB (>= 0)
    float attackCoeff = 1.0f;       // open-loop one-pole coefficients
    float releaseCoeff = 1.0f;
    RatioCurve curve { 4.0f, -10.0f, 6.0f, true };
};
