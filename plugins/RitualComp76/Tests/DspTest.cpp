// Offline checks for the RitualComp76 FET core (Phase 4.1.1).
// Build: cmake --build build --target RitualComp76_DspTest && ./build/plugins/RitualComp76/RitualComp76_DspTest_artefacts/Release/RitualComp76_DspTest
#include <juce_audio_basics/juce_audio_basics.h>
#include "FetCompressor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what, double got, double want, double tol)
    {
        std::printf ("%s  %-52s got %9.3f  want %9.3f (±%.2f)\n", ok ? "PASS" : "FAIL", what, got, want, tol);
        if (! ok) ++failures;
    }

    void near (const char* what, double got, double want, double tol) { check (std::abs (got - want) <= tol, what, got, want, tol); }

    // Run a stereo sine of `levelDb` peak through the compressor; return final GR.
    float steadyStateGr (RatioMode mode, float levelDb, double fs, float attack = 4.0f, float release = 5.0f, float seconds = 2.0f)
    {
        FetCompressor c;
        c.prepare (fs);
        c.setSettings ({ attack, release, mode });
        const auto amp = juce::Decibels::decibelsToGain (levelDb);
        const int block = 256;
        std::vector<float> l ((size_t) block), r ((size_t) block);
        float* ch[] = { l.data(), r.data() };
        double phase = 0.0;
        const auto inc = juce::MathConstants<double>::twoPi * 1000.0 / fs;
        for (int done = 0; done < (int) (seconds * fs); done += block)
        {
            for (size_t n = 0; n < (size_t) block; ++n) { l[n] = r[n] = amp * (float) std::sin (phase); phase += inc; }
            c.process (ch, 2, block);
        }
        return c.getCurrentGainReductionDb();
    }

    // Static curve expectation (above the knee): GR = (1 - 1/R) * (L - T)
    double expectedGr (RatioMode mode, double levelDb)
    {
        const auto c = FetCompressor::curveFor (mode);
        return std::max (0.0, (1.0 - 1.0 / c.ratio) * (levelDb - c.thresholdDb));
    }
}

int main()
{
    std::printf ("== Static curve (1 kHz sine, 48 kHz) ==\n");
    for (auto mode : { RatioMode::R4, RatioMode::R8, RatioMode::R12, RatioMode::R20 })
    {
        const auto c = FetCompressor::curveFor (mode);
        for (float level : { c.thresholdDb + 6.0f, c.thresholdDb + 12.0f })
        {
            char label[96];
            std::snprintf (label, sizeof label, "ratio %2.0f:1, %+5.1f dB over thr", c.ratio, level - c.thresholdDb);
            // peak detector on a sine settles slightly under the full static value due to release ripple
            near (label, steadyStateGr (mode, level, 48000.0), expectedGr (mode, level), 0.6);
        }
    }

    std::printf ("\n== Below threshold / Out mode ==\n");
    near ("4:1 at thr-12 dB -> no GR", steadyStateGr (RatioMode::R4, -22.0f, 48000.0), 0.0, 0.05);
    near ("Out mode, +20 dB hot -> no GR", steadyStateGr (RatioMode::Out, 10.0f, 48000.0), 0.0, 0.0);

    std::printf ("\n== Stability: fastest attack, every ratio, 44.1k / 192k ==\n");
    for (double fs : { 44100.0, 192000.0 })
        for (auto mode : { RatioMode::R4, RatioMode::R20, RatioMode::All })
        {
            const auto gr = steadyStateGr (mode, 12.0f, fs, 7.0f, 7.0f, 0.5f);
            char label[96];
            std::snprintf (label, sizeof label, "fs %6.0f ratio %d attack 7: finite & bounded", fs, (int) mode);
            check (std::isfinite (gr) && gr >= 0.0f && gr < 40.0f, label, gr, expectedGr (mode == RatioMode::All ? RatioMode::R20 : mode, 12.0), 40.0);
        }

    std::printf ("\n== Ballistics (step response, 48 kHz, DC step into 20:1) ==\n");
    {
        // Attack: time to reach 63%% of final GR after a +20 dB step.
        for (float step : { 1.0f, 7.0f })
        {
            const double fs = 48000.0;
            FetCompressor c; c.prepare (fs); c.setSettings ({ step, 4.0f, RatioMode::R20 });
            const float target = (float) expectedGr (RatioMode::R20, 14.0);
            float x = juce::Decibels::decibelsToGain (14.0f);
            int n = 0;
            for (; n < (int) fs; ++n)
            {
                float s = x; float* ch[] = { &s };
                c.process (ch, 1, 1);
                if (c.getCurrentGainReductionDb() >= 0.632f * target) break;
            }
            const double ms = 1000.0 * n / fs, want = FetCompressor::attackSecondsFromStep (step) * 1000.0;
            char label[96]; std::snprintf (label, sizeof label, "attack step %.0f time constant (ms)", step);
            near (label, ms, want, std::max (0.03, want * 0.35));
        }

        // Release: from steady GR, input drops to silence; time to fall to 37%%.
        for (float step : { 1.0f, 7.0f })
        {
            const double fs = 48000.0;
            FetCompressor c; c.prepare (fs); c.setSettings ({ 7.0f, step, RatioMode::R20 });
            float s;
            for (int n = 0; n < 4800; ++n) { s = juce::Decibels::decibelsToGain (14.0f); float* ch[] = { &s }; c.process (ch, 1, 1); }
            const float start = c.getCurrentGainReductionDb();
            int n = 0;
            for (; n < (int) (3 * fs); ++n)
            {
                s = 0.0f; float* ch[] = { &s };
                c.process (ch, 1, 1);
                if (c.getCurrentGainReductionDb() <= 0.368f * start) break;
            }
            const double ms = 1000.0 * n / fs, want = FetCompressor::releaseSecondsFromStep (step) * 1000.0;
            char label[96]; std::snprintf (label, sizeof label, "release step %.0f time constant (ms)", step);
            near (label, ms, want, want * 0.1);
        }
    }

    std::printf ("\n== Stereo link ==\n");
    {
        FetCompressor c; c.prepare (48000.0); c.setSettings ({ 4.0f, 5.0f, RatioMode::R4 });
        std::vector<float> l (48000u), r (48000u);
        for (size_t n = 0; n < 48000u; ++n) { l[n] = 0.9f * (float) std::sin ((double) n * 0.1); r[n] = 0.01f * (float) std::sin ((double) n * 0.1); }
        float* ch[] = { l.data(), r.data() };
        c.process (ch, 2, 48000);
        const double ratioOut = l[47990] != 0.0f ? r[47990] / l[47990] : 0.0;
        near ("quiet channel keeps L/R ratio (image stable)", ratioOut, 0.01 / 0.9, 1.0e-4);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
