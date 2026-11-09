// Offline checks for RitualAnvil DSP (Phase 4.1.1).
// Build: cmake --build build --target RitualAnvil_DspTest
// Run:   ./build/plugins/RitualAnvil/RitualAnvil_DspTest_artefacts/Release/RitualAnvil_DspTest
#include <juce_audio_basics/juce_audio_basics.h>
#include "PluginProcessor.h"
#include "MacroMapper.h"
#include "BandSplitter3.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace
{
    int failures = 0;
    constexpr double fs = 48000.0;

    void check (bool ok, const char* what, double got, double want, double tol)
    {
        std::printf ("%s  %-58s got %10.4f  want %10.4f (±%.4g)\n", ok ? "PASS" : "FAIL", what, got, want, tol);
        if (! ok) ++failures;
    }
    void near (const char* what, double got, double want, double tol) { check (std::abs (got - want) <= tol, what, got, want, tol); }
    void atMost (const char* what, double got, double max) { check (got <= max, what, got, max, 0.0); }

    using Vec = std::vector<float>;

    double toneMag (const Vec& x, double freq, size_t start = 0)
    {
        const auto n = x.size() - start;
        const auto w = juce::MathConstants<double>::twoPi * freq / fs;
        const auto c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (size_t i = start; i < x.size(); ++i) { const auto s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
        return 2.0 * std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) n;
    }

    double toDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

    //--------------------------------------------------------------------------
    struct Rig
    {
        std::unique_ptr<RitualAnvilAudioProcessor> proc = std::make_unique<RitualAnvilAudioProcessor>();

        void set (const char* id, float value)
        {
            auto* p = proc->apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (value));
        }

        void prepare (int block = 512) { proc->prepareToPlay (fs, block); }

        /** Runs L/R through the processor in `block`-sized calls, returns the output. */
        std::pair<Vec, Vec> run (const Vec& inL, const Vec& inR, int block = 512)
        {
            Vec l = inL, r = inR;
            juce::MidiBuffer midi;
            for (size_t off = 0; off < l.size(); off += (size_t) block)
            {
                const auto n = (int) std::min ((size_t) block, l.size() - off);
                float* ch[] = { l.data() + off, r.data() + off };
                juce::AudioBuffer<float> buf (ch, 2, n);
                proc->processBlock (buf, midi);
            }
            return { l, r };
        }
    };

    Vec sine (double freq, float amp, size_t len)
    {
        Vec v (len);
        for (size_t i = 0; i < len; ++i)
            v[i] = amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (double) i / fs);
        return v;
    }

    /** Steady-state gain (dB) of the full chain for a mono-compatible sine at `freq`. */
    double chainGainDb (Rig& rig, double freq, float ampDb = -18.0f)
    {
        const auto len = (size_t) fs;   // 1 s, measure last 0.5 s
        const auto amp = juce::Decibels::decibelsToGain (ampDb);
        const auto x = sine (freq, amp, len);
        const auto [l, r] = rig.run (x, x);
        return toDb (toneMag (l, freq, len / 2) / amp);
    }

    //==========================================================================
    void testMacroMapper()
    {
        std::puts ("\n-- MacroMapper (additive law) --");
        using namespace anvil;
        auto m = defaultParams();
        auto at = [&] (float pct) { auto p = m; p[MixReady] = pct; return applyMacro (p); };

        const auto e0 = at (0.0f);
        bool identical = true;
        for (int i = 0; i < NumSliderParams; ++i) identical &= std::abs (e0[(size_t) i] - m[(size_t) i]) < 1.0e-6f;
        check (identical, "t=0: effective == manual (all params)", identical, 1, 0);

        const auto e1 = at (100.0f);
        near ("t=1: HPF floor 50 Hz (manual 40)", e1[Hpf], 50.0, 1e-4);
        near ("t=1: Kick +4 dB", e1[EqKick], 4.0, 1e-4);
        near ("t=1: Box -3 dB", e1[EqBox], -3.0, 1e-4);
        near ("t=1: Snare +3 dB", e1[EqSnare], 3.0, 1e-4);
        near ("t=1: Presence +2 dB", e1[EqPresence], 2.0, 1e-4);
        near ("t=1: Air +4 dB", e1[EqAir], 4.0, 1e-4);
        near ("t=1: Punch 100 % (blend 0.5)", e1[Punch], 100.0, 1e-3);
        near ("t=1: Heat 6 dB", e1[Heat], 6.0, 1e-4);
        near ("t=1: Width 120 %", e1[Width], 120.0, 1e-4);
        near ("t=1: Attack +4 dB", e1[TransientAttack], 4.0, 1e-4);
        near ("t=1: Sustain 0 dB", e1[TransientSustain], 0.0, 1e-4);
        near ("t=1: Ceiling untouched", e1[Ceiling], -0.3, 1e-6);
        near ("t=1: Input untouched", e1[InputGain], 0.0, 1e-6);

        near ("t=0.25: Sustain dip -3 dB", at (25.0f)[TransientSustain], -3.0, 1e-4);
        near ("t=0.01: Sustain continuous (~-0.12 dB)", at (1.0f)[TransientSustain], -0.12, 1e-3);
        near ("t=0.01: HPF continuous (40 Hz, floor 20.3)", at (1.0f)[Hpf], 40.0, 1e-4);
        near ("t=0.5: Sustain back to 0", at (50.0f)[TransientSustain], 0.0, 1e-4);
        near ("t=0.5: Attack 0", at (50.0f)[TransientAttack], 0.0, 1e-4);
        near ("t=0.5: Width 100 %", at (50.0f)[Width], 100.0, 1e-4);
        near ("t=0.4: Punch 0 %", at (40.0f)[Punch], 0.0, 1e-4);
        near ("t=0.6: Heat 0 dB", at (60.0f)[Heat], 0.0, 1e-4);
        near ("t=0.75: Punch 58.3 %", at (75.0f)[Punch], 58.333, 1e-2);

        auto clampCase = m;
        clampCase[MixReady] = 100.0f; clampCase[EqAir] = 12.0f; clampCase[Hpf] = 80.0f;
        clampCase[Punch] = 80.0f; clampCase[Heat] = 18.0f; clampCase[Width] = 145.0f; clampCase[EqBox] = -11.0f;
        const auto ec = applyMacro (clampCase);
        near ("clamp: Air 12 + 4 -> 12", ec[EqAir], 12.0, 1e-6);
        near ("clamp: HPF manual 80 above floor -> 80", ec[Hpf], 80.0, 1e-6);
        near ("clamp: Punch 80 % + macro -> 100 %", ec[Punch], 100.0, 1e-3);
        near ("clamp: Heat 18 + 6 -> 20", ec[Heat], 20.0, 1e-6);
        near ("clamp: Width 145 + 20 -> 150", ec[Width], 150.0, 1e-6);
        near ("clamp: Box -11 - 3 -> -12", ec[EqBox], -12.0, 1e-6);
    }

    void testBandSplitterFlat()
    {
        std::puts ("\n-- BandSplitter3 (LR4 + allpass comp) sums flat --");
        for (auto [f1, f2] : { std::pair { 300.0f, 20000.0f }, std::pair { 200.0f, 5000.0f } })
        {
            double worst = 0.0;
            for (double f : { 30.0, 100.0, 200.0, 300.0, 1000.0, 3000.0, 5000.0, 10000.0, 18000.0 })
            {
                anvil::BandSplitter3 s;
                s.prepare (fs, 512, f1, f2);
                const auto x = sine (f, 0.5f, (size_t) fs);
                Vec y (x.size());
                for (size_t i = 0; i < x.size(); ++i) { float lo, mi, hi; s.process (0, x[i], lo, mi, hi); y[i] = lo + mi + hi; }
                worst = std::max (worst, std::abs (toDb (toneMag (y, f, y.size() / 2) / 0.5)));
            }
            char label[96];
            std::snprintf (label, sizeof label, "split %.0f/%.0f Hz: worst |sum| deviation (dB)", f1, f2);
            atMost (label, worst, 0.01);
        }
    }

    void testNeutralChain()
    {
        std::puts ("\n-- Full chain at defaults is transparent (above the 40 Hz HPF) --");
        for (double f : { 200.0, 1000.0, 5000.0, 12000.0 })
        {
            Rig rig; rig.prepare();
            char label[64]; std::snprintf (label, sizeof label, "defaults: gain @ %.0f Hz (dB)", f);
            near (label, chainGainDb (rig, f), 0.0, 0.1);
        }
        Rig rig; rig.prepare();
        near ("defaults: HPF -3 dB point region @ 40 Hz (dB)", chainGainDb (rig, 40.0), -3.0, 0.3);
    }

    void testToneEQ()
    {
        std::puts ("\n-- Tone EQ bands hit their targets --");
        const char* ids[] = { ParameterIDs::eqKick, ParameterIDs::eqBox, ParameterIDs::eqSnare, ParameterIDs::eqPresence, ParameterIDs::eqAir };
        for (int b = 0; b < 5; ++b)
        {
            Rig rig;
            rig.set (ParameterIDs::hpf, 20.0f);
            rig.set (ids[b], 6.0f);
            rig.prepare();
            const double f = anvil::fixed::eqFreq[b];
            const double hpfLoss = b == 0 ? -0.10 : 0.0;   // 20 Hz HPF still shaves ~0.1 dB at 60 Hz
            char label[64]; std::snprintf (label, sizeof label, "%s +6 dB @ %.0f Hz", ids[b], f);
            near (label, chainGainDb (rig, f), 6.0 + hpfLoss, 0.2);
        }
    }

    void testMacroInChain()
    {
        std::puts ("\n-- Mix Ready drives the chain (effective values exported) --");
        Rig rig;
        rig.set (ParameterIDs::mixReady, 100.0f);
        rig.prepare();
        chainGainDb (rig, 1000.0);
        near ("eff Kick exported", rig.proc->getEffectiveValue (anvil::EqKick), 4.0, 1e-4);
        near ("eff Heat exported", rig.proc->getEffectiveValue (anvil::Heat), 6.0, 1e-4);
        near ("eff Width exported", rig.proc->getEffectiveValue (anvil::Width), 120.0, 1e-4);

        Rig ref; ref.set (ParameterIDs::hpf, 20.0f); ref.prepare();
        Rig hot; hot.set (ParameterIDs::hpf, 20.0f); hot.set (ParameterIDs::mixReady, 50.0f); hot.prepare();
        // At 50 %: Air +2 dB (12 kHz), no punch/heat yet → quiet sine shows the EQ lift
        const double lift = chainGainDb (hot, 12000.0, -30.0f) - chainGainDb (ref, 12000.0, -30.0f);
        near ("mix 50 %: 12 kHz lift ≈ Air +2 (+Pres tail)", lift, 2.35, 0.4);
    }

    void testLimiter()
    {
        std::puts ("\n-- Placeholder limiter holds the ceiling (sample peak) --");
        for (float ceil : { -0.1f, -0.3f, -1.0f })
        {
            Rig rig;
            rig.set (ParameterIDs::inputGain, 12.0f);
            rig.set (ParameterIDs::ceiling, ceil);
            rig.prepare();
            const auto x = sine (100.0, 0.9f, (size_t) fs);
            const auto [l, r] = rig.run (x, x);
            float pk = 0.0f;
            for (size_t i = l.size() / 4; i < l.size(); ++i) pk = std::max ({ pk, std::abs (l[i]), std::abs (r[i]) });
            char label[64]; std::snprintf (label, sizeof label, "ceiling %.1f dB: output peak (dBFS)", ceil);
            atMost (label, toDb (pk), ceil + 0.01);
        }
    }

    void testWidth()
    {
        std::puts ("\n-- Width: side gain above 200 Hz, lows untouched --");
        auto sideGainDb = [] (double f, float widthPct)
        {
            Rig rig; rig.set (ParameterIDs::hpf, 20.0f); rig.set (ParameterIDs::width, widthPct); rig.prepare();
            const auto s = sine (f, 0.1f, (size_t) fs);
            Vec neg (s.size());
            for (size_t i = 0; i < s.size(); ++i) neg[i] = -s[i];
            const auto [l, r] = rig.run (s, neg);   // pure side signal
            return toDb (toneMag (l, f, l.size() / 2) / 0.1);
        };
        near ("width 150 %: side @ 5 kHz (dB)", sideGainDb (5000.0, 150.0f), toDb (1.5), 0.1);
        near ("width 150 %: side @ 40 Hz (dB, untouched; +3.5 if not)", sideGainDb (40.0, 150.0f), 0.0, 0.3);
        near ("width 100 %: side @ 1 kHz identity (dB)", sideGainDb (1000.0, 100.0f), 0.0, 0.02);
    }

    void testBlockSizeInvariance()
    {
        std::puts ("\n-- Output independent of host block size --");
        auto render = [] (int block)
        {
            Rig rig;
            rig.set (ParameterIDs::mixReady, 70.0f);
            rig.set (ParameterIDs::heat, 8.0f);
            rig.set (ParameterIDs::transientAttack, 6.0f);
            rig.prepare (512);
            juce::Random rnd (42);
            Vec l ((size_t) fs / 2), r (l.size());
            for (size_t i = 0; i < l.size(); ++i)
            {
                const float env = std::exp (-(float) (i % 12000) / 2000.0f);
                l[i] = env * (rnd.nextFloat() * 2.0f - 1.0f) * 0.5f;
                r[i] = env * (rnd.nextFloat() * 2.0f - 1.0f) * 0.5f;
            }
            return rig.run (l, r, block).first;
        };
        const auto a = render (512), b = render (1), c = render (1000);   // 1000 > prepared size: chunked
        double d1 = 0, d2 = 0;
        for (size_t i = 0; i < a.size(); ++i) { d1 = std::max (d1, (double) std::abs (a[i] - b[i])); d2 = std::max (d2, (double) std::abs (a[i] - c[i])); }
        atMost ("max |512 - 1-sample blocks|", d1, 1.0e-5);
        atMost ("max |512 - 1000-sample blocks|", d2, 1.0e-5);
    }

    void testExtremesFinite()
    {
        std::puts ("\n-- Extremes stay finite and under the ceiling --");
        Rig rig;
        for (int i = 0; i < anvil::NumSliderParams; ++i)
            rig.set (anvil::sliderSpecs[(size_t) i].id, anvil::sliderSpecs[(size_t) i].max);
        rig.prepare();
        const auto x = sine (55.0, 1.0f, (size_t) fs);
        const auto [l, r] = rig.run (x, x);
        bool finite = true; float pk = 0.0f;
        for (size_t i = 0; i < l.size(); ++i) { finite &= std::isfinite (l[i]) && std::isfinite (r[i]); pk = std::max ({ pk, std::abs (l[i]), std::abs (r[i]) }); }
        check (finite, "all params max: output finite", finite, 1, 0);
        atMost ("all params max: peak (dBFS)", toDb (pk), -0.1 + 0.01);

        Rig silent; silent.prepare();
        const auto [sl, sr] = silent.run (Vec ((size_t) fs / 4, 0.0f), Vec ((size_t) fs / 4, 0.0f));
        float spk = 0.0f;
        for (auto v : sl) spk = std::max (spk, std::abs (v));
        atMost ("silence in -> silence out (peak)", spk, 1.0e-7);
    }

    void testBypass()
    {
        std::puts ("\n-- Bypass passes input untouched --");
        Rig rig;
        rig.set (ParameterIDs::mixReady, 100.0f);
        rig.set (ParameterIDs::bypass, 1.0f);
        rig.prepare();
        const auto x = sine (440.0, 0.5f, 48000);
        const auto [l, r] = rig.run (x, x);
        double d = 0;
        for (size_t i = 0; i < x.size(); ++i) d = std::max (d, (double) std::abs (l[i] - x[i]));
        atMost ("bypassed: max |out - in|", d, 0.0);
    }

    void testState()
    {
        std::puts ("\n-- State save / restore --");
        Rig a; a.set (ParameterIDs::heat, 7.5f); a.set (ParameterIDs::eqBox, -4.0f); a.set (ParameterIDs::hpf, 63.0f);
        juce::MemoryBlock mb; a.proc->getStateInformation (mb);
        Rig b; b.proc->setStateInformation (mb.getData(), (int) mb.getSize());
        near ("restored heat", b.proc->apvts.getRawParameterValue (ParameterIDs::heat)->load(), 7.5, 0.01);
        near ("restored eq_box", b.proc->apvts.getRawParameterValue (ParameterIDs::eqBox)->load(), -4.0, 0.01);
        near ("restored hpf", b.proc->apvts.getRawParameterValue (ParameterIDs::hpf)->load(), 63.0, 0.05);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    testMacroMapper();
    testBandSplitterFlat();
    testNeutralChain();
    testToneEQ();
    testMacroInChain();
    testLimiter();
    testWidth();
    testBlockSizeInvariance();
    testExtremesFinite();
    testBypass();
    testState();

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
