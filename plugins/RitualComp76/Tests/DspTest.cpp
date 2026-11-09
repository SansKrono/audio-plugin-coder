// Offline checks for RitualComp76 DSP (Phases 4.1.1 + 4.1.2).
// Build: cmake --build build --target RitualComp76_DspTest
// Run:   ./build/plugins/RitualComp76/RitualComp76_DspTest_artefacts/Release/RitualComp76_DspTest
#include <juce_audio_basics/juce_audio_basics.h>
#include "FetCompressor.h"
#include "Saturation.h"
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what, double got, double want, double tol)
    {
        std::printf ("%s  %-56s got %10.4f  want %10.4f (±%.4g)\n", ok ? "PASS" : "FAIL", what, got, want, tol);
        if (! ok) ++failures;
    }
    void near (const char* what, double got, double want, double tol) { check (std::abs (got - want) <= tol, what, got, want, tol); }
    void atLeast (const char* what, double got, double min) { check (got >= min, what, got, min, 0.0); }
    void atMost (const char* what, double got, double max) { check (got <= max, what, got, max, 0.0); }

    using Vec = std::vector<float>;

    // Goertzel magnitude of `freq` in x (amplitude-normalised)
    double toneMag (const Vec& x, double freq, double fs, size_t start = 0)
    {
        const auto n = x.size() - start;
        const auto w = juce::MathConstants<double>::twoPi * freq / fs;
        const auto c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (size_t i = start; i < x.size(); ++i) { const auto s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
        return 2.0 * std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) n;
    }

    Vec sine (double freq, double fs, float amp, size_t len)
    {
        Vec v (len);
        for (size_t i = 0; i < len; ++i)
            v[i] = amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (double) i / fs);
        return v;
    }

    float runSine (FetCompressor& c, float levelDb, double fs, float seconds, Vec* captureLast = nullptr)
    {
        const int block = 256;
        Vec l ((size_t) block), r ((size_t) block);
        float* ch[] = { l.data(), r.data() };
        const auto amp = juce::Decibels::decibelsToGain (levelDb);
        const auto inc = juce::MathConstants<double>::twoPi * 1000.0 / fs;
        double phase = 0.0;
        const int total = (int) (seconds * fs);
        if (captureLast) captureLast->clear();
        for (int done = 0; done < total; done += block)
        {
            for (size_t n = 0; n < (size_t) block; ++n) { l[n] = r[n] = amp * (float) std::sin (phase); phase += inc; }
            c.process (ch, 2, block);
            if (captureLast && done >= total - (int) (0.2 * fs)) captureLast->insert (captureLast->end(), l.begin(), l.end());
        }
        return c.getCurrentGainReductionDb();
    }

    float steadyStateGr (RatioMode mode, float levelDb, double fs, float attack = 4.0f, float release = 5.0f, float seconds = 2.0f)
    {
        FetCompressor c;
        c.prepare (fs);
        c.setSettings ({ attack, release, mode });
        return runSine (c, levelDb, fs, seconds);
    }

    double expectedGr (RatioMode mode, double levelDb)
    {
        const auto c = FetCompressor::curveFor (mode);
        return std::max (0.0, (1.0 - 1.0 / c.ratio) * (levelDb - c.thresholdDb));
    }

    // Release time (to 37%) after holding a +14 dB DC level for `holdSec` at 20:1.
    double releaseTimeMs (float releaseStep, double holdSec, double fs = 48000.0)
    {
        FetCompressor c; c.prepare (fs); c.setSettings ({ 7.0f, releaseStep, RatioMode::R20 });
        float s;
        for (int n = 0; n < (int) (holdSec * fs); ++n) { s = juce::Decibels::decibelsToGain (14.0f); float* ch[] = { &s }; c.process (ch, 1, 1); }
        const float start = c.getCurrentGainReductionDb();
        int n = 0;
        for (; n < (int) (10 * fs); ++n)
        {
            s = 0.0f; float* ch[] = { &s };
            c.process (ch, 1, 1);
            if (c.getCurrentGainReductionDb() <= 0.368f * start) break;
        }
        return 1000.0 * n / fs;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // processor tests need the message manager

    std::printf ("== Static curve (1 kHz sine, 48 kHz) ==\n");
    for (auto mode : { RatioMode::R4, RatioMode::R8, RatioMode::R12, RatioMode::R20 })
    {
        const auto c = FetCompressor::curveFor (mode);
        for (float level : { c.thresholdDb + 6.0f, c.thresholdDb + 12.0f })
        {
            char label[96];
            std::snprintf (label, sizeof label, "ratio %2.0f:1, %+5.1f dB over thr", c.ratio, level - c.thresholdDb);
            near (label, steadyStateGr (mode, level, 48000.0), expectedGr (mode, level), 0.6);
        }
    }

    std::printf ("\n== Below threshold / Out mode ==\n");
    near ("4:1 at thr-12 dB -> no GR", steadyStateGr (RatioMode::R4, -22.0f, 48000.0), 0.0, 0.05);
    near ("Out mode, +20 dB hot -> no GR", steadyStateGr (RatioMode::Out, 10.0f, 48000.0), 0.0, 0.0);

    std::printf ("\n== Stability: fastest attack, every ratio, 44.1k / 192k / 768k (4x of 192k) ==\n");
    for (double fs : { 44100.0, 192000.0, 768000.0 })
        for (auto mode : { RatioMode::R4, RatioMode::R20, RatioMode::All })
        {
            const auto gr = steadyStateGr (mode, 12.0f, fs, 7.0f, 7.0f, 0.5f);
            char label[96];
            std::snprintf (label, sizeof label, "fs %6.0f ratio %d attack 7: finite & bounded", fs, (int) mode);
            check (std::isfinite (gr) && gr >= 0.0f && gr < 60.0f, label, gr, 30.0, 30.0);
        }

    std::printf ("\n== Attack ballistics (DC step into 20:1, 48 kHz) ==\n");
    for (float step : { 1.0f, 7.0f })
    {
        const double fs = 48000.0;
        FetCompressor c; c.prepare (fs); c.setSettings ({ step, 4.0f, RatioMode::R20 });
        const float target = (float) expectedGr (RatioMode::R20, 14.0);
        int n = 0;
        for (; n < (int) fs; ++n)
        {
            float s = juce::Decibels::decibelsToGain (14.0f); float* ch[] = { &s };
            c.process (ch, 1, 1);
            if (c.getCurrentGainReductionDb() >= 0.632f * target) break;
        }
        const double ms = 1000.0 * n / fs, want = FetCompressor::attackSecondsFromStep (step) * 1000.0;
        char label[96]; std::snprintf (label, sizeof label, "attack step %.0f time constant (ms)", step);
        near (label, ms, want, std::max (0.03, want * 0.35));
    }

    std::printf ("\n== Program-dependent release (4.1.2) ==\n");
    for (float step : { 1.0f, 7.0f })
    {
        const double want = FetCompressor::releaseSecondsFromStep (step) * 1000.0;
        char label[96];
        std::snprintf (label, sizeof label, "release step %.0f after 2 ms burst ~ knob time (ms)", step);
        near (label, releaseTimeMs (step, 0.002), want, want * 0.1);
        std::snprintf (label, sizeof label, "release step %.0f after 3 s heavy GR >= 1.5x (ms)", step);
        atLeast (label, releaseTimeMs (step, 3.0), want * 1.5);
    }

    std::printf ("\n== All-buttons-in (4.1.2) ==\n");
    {
        const auto all20 = steadyStateGr (RatioMode::All, 6.0f, 48000.0);
        const auto r20   = steadyStateGr (RatioMode::R20, 6.0f, 48000.0);
        atLeast ("All mode squashes harder than 20:1 at +6 dBFS (dB GR)", all20, r20 + 1.0);

        // Overshoot: peak GR during a burst exceeds the settled GR
        FetCompressor c; c.prepare (48000.0); c.setSettings ({ 4.0f, 7.0f, RatioMode::All });
        float peakGr = 0.0f;
        for (int n = 0; n < 4800; ++n)
        {
            float s = juce::Decibels::decibelsToGain (0.0f); float* ch[] = { &s };
            c.process (ch, 1, 1);
            peakGr = juce::jmax (peakGr, c.getCurrentGainReductionDb());
        }
        atLeast ("All mode attack overshoot: peak/settled GR ratio", peakGr / juce::jmax (0.01f, c.getCurrentGainReductionDb()), 1.1);
    }

    std::printf ("\n== FET distortion (2nd harmonic of 1 kHz, 48 kHz) ==\n");
    {
        const double fs = 48000.0;
        auto h2Ratio = [&] (RatioMode mode, float levelDb)
        {
            FetCompressor c; c.prepare (fs); c.setSettings ({ 4.0f, 5.0f, mode });
            Vec out; runSine (c, levelDb, fs, 1.0f, &out);
            out.resize (out.size() - out.size() % 48);   // whole 1 kHz periods: no Goertzel leakage
            return toneMag (out, 2000.0, fs) / toneMag (out, 1000.0, fs);
        };
        const auto clean = h2Ratio (RatioMode::R4, -30.0f);   // below threshold, no GR
        const auto comp  = h2Ratio (RatioMode::R4, 0.0f);     // ~7.5 dB GR
        const auto all   = h2Ratio (RatioMode::All, 0.0f);
        atMost  ("no GR -> H2/H1 negligible", clean, 1.0e-4);
        atLeast ("4:1 with GR -> audible H2/H1", comp, 0.003);
        atLeast ("All mode -> H2 at least 2x 4:1", all / comp, 2.0);
    }

    std::printf ("\n== Colour stages: unity small-signal gain, bounded large-signal ==\n");
    {
        const double fs = 192000.0;
        rc76::InputTransformer t; t.prepare (fs);
        rc76::ClassAStage a;
        rc76::DcBlocker dc; dc.prepare (fs);
        auto in = sine (1000.0, fs, 0.01f, 96000);
        Vec outT (in.size()), outA (in.size());
        for (size_t i = 0; i < in.size(); ++i) { outT[i] = t.process (0, in[i]); outA[i] = dc.process (0, a.process (in[i])); }
        near ("transformer gain at -40 dBFS, 1 kHz", toneMag (outT, 1000.0, fs, 48000) / 0.01, 1.0, 0.01);
        near ("class-A gain at -40 dBFS, 1 kHz", toneMag (outA, 1000.0, fs, 48000) / 0.01, 1.0, 0.01);

        rc76::InputTransformer t2; t2.prepare (fs);
        float maxOut = 0.0f;
        for (auto x : sine (50.0, fs, 20.0f, 96000)) maxOut = juce::jmax (maxOut, std::abs (t2.process (0, x)));
        atMost ("transformer at +26 dBFS, 50 Hz: output bounded", maxOut, 12.0);
    }

    std::printf ("\n== Stereo link ==\n");
    {
        FetCompressor c; c.prepare (48000.0); c.setSettings ({ 4.0f, 5.0f, RatioMode::R4 });
        Vec l (48000u), r (48000u);
        for (size_t n = 0; n < 48000u; ++n) { l[n] = 0.9f * (float) std::sin ((double) n * 0.1); r[n] = 0.01f * (float) std::sin ((double) n * 0.1); }
        float* ch[] = { l.data(), r.data() };
        c.process (ch, 2, 48000);
        // FET distortion adds a little even-harmonic content per channel, so compare via fundamental
        near ("quiet channel keeps L/R level ratio (image stable)",
              toneMag (r, 48000.0 * 0.1 / juce::MathConstants<double>::twoPi, 48000.0, 24000) /
              toneMag (l, 48000.0 * 0.1 / juce::MathConstants<double>::twoPi, 48000.0, 24000),
              0.01 / 0.9, 5.0e-4);
    }

    std::printf ("\n== Processor (oversampling, latency, bypass, state) ==\n");
    {
        const double fs = 48000.0;
        const int block = 480;
        RitualComp76AudioProcessor p;
        p.setPlayConfigDetails (2, 2, fs, block);
        p.prepareToPlay (fs, block);
        const auto latency = p.getLatencySamples();
        check (latency > 0 && latency < 64, "reports oversampling latency (samples)", latency, 10, 64);

        auto setParam = [&] (const char* id, float value)
        {
            auto* prm = p.apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        };

        // Bypass: output == input delayed by exactly `latency` samples (phase aligned)
        setParam (ParameterIDs::bypass, 1.0f);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, block);
        Vec in, out;
        for (int b = 0; b < 20; ++b)
        {
            for (int n = 0; n < block; ++n)
            {
                const auto x = (float) std::sin (0.013 * (double) (b * block + n)) * 0.5f;
                buf.setSample (0, n, x); buf.setSample (1, n, x);
                in.push_back (x);
            }
            p.processBlock (buf, midi);
            for (int n = 0; n < block; ++n) out.push_back (buf.getSample (0, n));
        }
        double maxErr = 0.0;
        for (size_t i = 4000; i < out.size(); ++i) maxErr = std::max (maxErr, (double) std::abs (out[i] - in[i - (size_t) latency]));
        near ("bypass = input delayed by latency (max abs error)", maxErr, 0.0, 1.0e-6);

        // Active: output finite, compression happening on a hot signal
        setParam (ParameterIDs::bypass, 0.0f);
        setParam (ParameterIDs::input, 0.0f);          // +24 dB drive
        setParam (ParameterIDs::ratio, (float) RatioMode::All);
        bool finite = true; float peak = 0.0f;
        for (int b = 0; b < 100; ++b)
        {
            for (int n = 0; n < block; ++n) { const auto x = (float) std::sin (0.13 * (double) (b * block + n)) * 0.5f; buf.setSample (0, n, x); buf.setSample (1, n, x); }
            p.processBlock (buf, midi);
            for (int n = 0; n < block; ++n) { finite &= std::isfinite (buf.getSample (0, n)); peak = juce::jmax (peak, std::abs (buf.getSample (0, n))); }
        }
        check (finite && peak > 0.0f, "All mode, +24 dB drive: output finite and non-silent", peak, 0.5, 10.0);
        atLeast ("meter reports GR (with All-mode pin)", p.popGainReductionDb(), 8.0);

        // Mute: input at bottom of travel -> silence
        setParam (ParameterIDs::input, ParameterRanges::attenMinDb);
        for (int b = 0; b < 100; ++b)   // 1 s: let DC-blocker / filter tails decay
        {
            for (int n = 0; n < block; ++n) { buf.setSample (0, n, 0.5f); buf.setSample (1, n, 0.5f); }
            p.processBlock (buf, midi);
        }
        atMost ("input at ∞ -> output silent (peak)", buf.getMagnitude (0, block), 1.0e-5);

        // Odd block sizes larger than prepared size are handled in chunks
        juce::AudioBuffer<float> big (2, 2000);
        big.clear();
        setParam (ParameterIDs::input, -24.0f);
        p.processBlock (big, midi);
        check (std::isfinite (big.getSample (0, 1999)), "block larger than prepared size processed", 0, 0, 0);

        // State round-trip
        setParam (ParameterIDs::attack, 6.5f);
        juce::MemoryBlock state; p.getStateInformation (state);
        RitualComp76AudioProcessor p2;
        p2.setStateInformation (state.getData(), (int) state.getSize());
        near ("state restore: attack", p2.apvts.getRawParameterValue (ParameterIDs::attack)->load(), 6.5, 0.01);
        near ("state restore: ratio = All", p2.apvts.getRawParameterValue (ParameterIDs::ratio)->load(), (double) RatioMode::All, 0.0);
    }

    std::printf ("\n== CPU (stereo, 48 kHz, 512-sample blocks, All mode, 4x OS) ==\n");
    {
        const double fs = 48000.0;
        const int block = 512, seconds = 20;
        RitualComp76AudioProcessor p;
        p.setPlayConfigDetails (2, 2, fs, block);
        p.prepareToPlay (fs, block);
        auto* ratio = p.apvts.getParameter (ParameterIDs::ratio);
        ratio->setValueNotifyingHost (ratio->convertTo0to1 ((float) RatioMode::All));
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        juce::Random rng (76);
        const auto blocks = (int) (seconds * fs / block);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < block; ++n)
                    buf.setSample (ch, n, (rng.nextFloat() * 2.0f - 1.0f) * 0.5f);
            p.processBlock (buf, midi);
        }
        const auto pct = 100.0 * (juce::Time::getMillisecondCounterHiRes() - t0) / (seconds * 1000.0);
        atMost ("realtime CPU use, one core (%)", pct, 3.0);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
