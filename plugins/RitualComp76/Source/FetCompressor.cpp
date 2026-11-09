#include "FetCompressor.h"

#include <cmath>

namespace
{
    constexpr float minLevel = 1.0e-9f;   // -180 dBFS floor for the detector

    inline float onePoleCoeff (float seconds, double fs) noexcept
    {
        return 1.0f - std::exp (-1.0f / (seconds * (float) fs));
    }
}

float FetCompressor::attackSecondsFromStep (float step) noexcept
{
    const auto t = juce::jlimit (0.0f, 1.0f, (step - 1.0f) / 6.0f);
    return 800.0e-6f * std::pow (20.0f / 800.0f, t);
}

float FetCompressor::releaseSecondsFromStep (float step) noexcept
{
    const auto t = juce::jlimit (0.0f, 1.0f, (step - 1.0f) / 6.0f);
    return 1.1f * std::pow (0.05f / 1.1f, t);
}

FetCompressor::RatioCurve FetCompressor::curveFor (RatioMode mode) noexcept
{
    // Threshold rises slightly with ratio and the knee hardens, as on the original unit.
    switch (mode)
    {
        case RatioMode::Out: return { 1.0f,  0.0f, 0.0f, false };
        case RatioMode::R4:  return { 4.0f, -10.0f, 6.0f, true };
        case RatioMode::R8:  return { 8.0f,  -8.0f, 4.0f, true };
        case RatioMode::R12: return { 12.0f, -7.0f, 3.0f, true };
        case RatioMode::R20: return { 20.0f, -6.0f, 2.0f, true };
        // Phase 4.1.2 replaces this with the full all-buttons-in model
        // (level-dependent ratio, knee overshoot, timing shift, extra distortion).
        case RatioMode::All: return { 20.0f, -8.0f, 2.0f, true };
    }
    return { 4.0f, -10.0f, 6.0f, true };
}

void FetCompressor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
    setSettings ({});
}

void FetCompressor::reset()
{
    grDb = 0.0f;
}

void FetCompressor::setSettings (const Settings& s)
{
    attackCoeff  = onePoleCoeff (attackSecondsFromStep (s.attackStep), sampleRate);
    releaseCoeff = onePoleCoeff (releaseSecondsFromStep (s.releaseStep), sampleRate);
    curve = curveFor (s.ratio);
}

float FetCompressor::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const auto ratioMinusOne = curve.ratio - 1.0f;
    const auto halfKnee = 0.5f * curve.kneeDb;
    float blockMaxGr = grDb;

    for (int n = 0; n < numSamples; ++n)
    {
        float error;
        float loopGain = 1.0f;

        if (curve.active)
        {
            // Linked full-wave peak detector
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (channels[ch][n]));

            // Feedback sense: level at the cell output = input level - current GR
            const auto levelDb = 20.0f * std::log10 (juce::jmax (peak, minLevel));
            const auto d = levelDb - grDb - curve.thresholdDb;

            float over, slope;
            if (d <= -halfKnee)      { over = 0.0f; slope = 0.0f; }
            else if (d >= halfKnee)  { over = d;    slope = 1.0f; }
            else
            {
                const auto x = d + halfKnee;
                over  = x * x / (2.0f * curve.kneeDb);
                slope = x / curve.kneeDb;
            }

            error = ratioMinusOne * over - grDb;
            loopGain = 1.0f + ratioMinusOne * slope;
        }
        else
        {
            error = -grDb;   // ratio "Out": let any remaining GR recover
        }

        const auto coeff = (error > 0.0f ? attackCoeff : releaseCoeff) / loopGain;
        grDb = juce::jmax (0.0f, grDb + coeff * error);
        blockMaxGr = juce::jmax (blockMaxGr, grDb);

        const auto gain = juce::Decibels::decibelsToGain (-grDb, -200.0f);
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][n] *= gain;
    }

    return blockMaxGr;
}
