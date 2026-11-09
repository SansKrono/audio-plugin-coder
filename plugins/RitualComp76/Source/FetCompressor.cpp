#include "FetCompressor.h"

#include <cmath>

namespace
{
    constexpr float minLevel = 1.0e-9f;      // -180 dBFS floor for the detector
    constexpr float fetAmountNormal = 0.04f;
    constexpr float fetAmountAll    = 0.12f;
    constexpr float fetMaxGrDb      = 30.0f;
    constexpr float sustainSeconds  = 0.5f;
    constexpr float allBumpSeconds  = 0.04f;

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
        case RatioMode::Out: return { 1.0f,   0.0f, 0.0f, false };
        case RatioMode::R4:  return { 4.0f, -10.0f, 6.0f, true };
        case RatioMode::R8:  return { 8.0f,  -8.0f, 4.0f, true };
        case RatioMode::R12: return { 12.0f, -7.0f, 3.0f, true };
        case RatioMode::R20: return { 20.0f, -6.0f, 2.0f, true };
        case RatioMode::All: return { 12.0f, -8.0f, 2.0f, true };   // base ratio; grows with level
    }
    return { 4.0f, -10.0f, 6.0f, true };
}

void FetCompressor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    sustainCoeff = onePoleCoeff (sustainSeconds, sampleRate);
    allBumpCoeff = onePoleCoeff (allBumpSeconds, sampleRate);
    reset();
    setSettings ({});
}

void FetCompressor::reset()
{
    grDb = 0.0f;
    sustainDb = 0.0f;
    grSlow = 0.0f;
    appliedGrDb = 0.0f;
}

void FetCompressor::setSettings (const Settings& s)
{
    allMode = s.ratio == RatioMode::All;
    curve = curveFor (s.ratio);

    const auto attackSec  = attackSecondsFromStep (s.attackStep)  * (allMode ? allAttackScale  : 1.0f);
    const auto releaseSec = releaseSecondsFromStep (s.releaseStep) * (allMode ? allReleaseScale : 1.0f);

    attackCoeff      = onePoleCoeff (attackSec, sampleRate);
    releaseFastCoeff = onePoleCoeff (releaseSec, sampleRate);
    releaseSlowCoeff = onePoleCoeff (releaseSec * slowReleaseFactor, sampleRate);
    fetAmount        = curve.active ? (allMode ? fetAmountAll : fetAmountNormal) : 0.0f;
}

float FetCompressor::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const auto halfKnee = 0.5f * curve.kneeDb;
    float blockMaxGr = grDb;

    for (int n = 0; n < numSamples; ++n)
    {
        float error;
        float loopGain = 1.0f;
        bool attacking = false;
        bool compressing = false;

        if (curve.active)
        {
            // Linked full-wave peak detector
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (channels[ch][n]));

            // Feedback sense: level at the cell output = input level - current GR
            const auto levelDb = 20.0f * std::log10 (juce::jmax (peak, minLevel));
            const auto d = levelDb - grDb - curve.thresholdDb;

            float over, kneeSlope;
            if (d <= -halfKnee)      { over = 0.0f; kneeSlope = 0.0f; }
            else if (d >= halfKnee)  { over = d;    kneeSlope = 1.0f; }
            else
            {
                const auto x = d + halfKnee;
                over      = x * x / (2.0f * curve.kneeDb);
                kneeSlope = x / curve.kneeDb;
            }

            // f(over) = (R - 1) * over, R level-dependent in All mode
            float target, targetSlope;
            if (allMode)
            {
                const auto r = curve.ratio + allRatioSlope * over;
                if (r < allRatioMax)
                {
                    target      = (r - 1.0f) * over;
                    targetSlope = (r - 1.0f) + allRatioSlope * over;
                }
                else
                {
                    target      = (allRatioMax - 1.0f) * over;
                    targetSlope = allRatioMax - 1.0f;
                }
            }
            else
            {
                target      = (curve.ratio - 1.0f) * over;
                targetSlope = curve.ratio - 1.0f;
            }

            compressing = over > 0.0f;
            attacking = target > grDb;

            error = target - grDb;
            loopGain = 1.0f + targetSlope * kneeSlope;
        }
        else
        {
            error = -grDb;   // ratio "Out": let any remaining GR recover
        }

        float coeff;
        if (attacking)
        {
            coeff = attackCoeff;
        }
        else
        {
            // Program-dependent release: sustained GR pushes towards the slow stage
            const auto w = juce::jlimit (0.0f, 1.0f, sustainDb / sustainFullDb);
            coeff = releaseFastCoeff + w * (releaseSlowCoeff - releaseFastCoeff);
        }

        grDb = juce::jmax (0.0f, grDb + (coeff / loopGain) * error);
        // Sustain builds up slowly only while the signal is actually being compressed,
        // and never exceeds the current GR, so a release cannot slow itself down.
        if (compressing)
            sustainDb += sustainCoeff * (grDb - sustainDb);
        sustainDb = juce::jmin (sustainDb, grDb);

        // All mode "bump": applied GR overshoots when GR jumps above its recent average,
        // then settles back. Kept outside the feedback loop so the loop can't cancel it.
        float appliedGr = grDb;
        if (allMode)
        {
            grSlow += allBumpCoeff * (grDb - grSlow);
            appliedGr += allOvershoot * juce::jmax (0.0f, grDb - grSlow);
        }
        else
        {
            grSlow = grDb;
        }
        appliedGrDb = appliedGr;
        blockMaxGr = juce::jmax (blockMaxGr, appliedGr);

        const auto gain = juce::Decibels::decibelsToGain (-appliedGr, -200.0f);
        const auto h = fetAmount * juce::jmin (appliedGr, fetMaxGrDb) * 0.1f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto y = channels[ch][n] * gain;
            const auto yc = juce::jlimit (-2.0f, 2.0f, y);
            channels[ch][n] = y + h * yc * yc;   // FET channel nonlinearity: even harmonics
        }
    }

    return blockMaxGr;
}
