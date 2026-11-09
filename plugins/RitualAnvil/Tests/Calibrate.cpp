// RitualAnvil calibration + listening renders (Phase 4.1.3). Not a pass/fail test.
// Build: cmake --build build --target RitualAnvil_Calibrate
// Run:   ./build/plugins/RitualAnvil/RitualAnvil_Calibrate <outDir> <drums1.wav> [drums2.wav ...]
//
// 1. Heat output compensation: level change of the Heat stage alone vs Drive,
//    with compensation OFF (raw) and ON (current law), K-weighted.
// 2. Mix Ready sweep through the full chain: K-weighted loudness, sample peak, true-peak-ish
//    sample peak; renders <outDir>/<stem>_mixNNN.wav for listening (loudness-matched copies too).
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    using Buf = juce::AudioBuffer<float>;

    /** BS.1770 K-weighting (RBJ approximations: +4 dB high shelf @ 1681 Hz, HPF 38 Hz), ungated mean square → LUFS-like. */
    double loudness (const Buf& b, double fs)
    {
        const auto shelf = [&] {
            const double A = std::pow (10.0, 4.0 / 40.0), w = 2.0 * juce::MathConstants<double>::pi * 1681.0 / fs;
            const double c = std::cos (w), al = std::sin (w) / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * al;
            const double a0 = (A + 1) - (A - 1) * c + sA;
            return anvil::BiquadCoeffs { A * ((A + 1) + (A - 1) * c + sA) / a0, -2 * A * ((A - 1) + (A + 1) * c) / a0,
                                         A * ((A + 1) + (A - 1) * c - sA) / a0, 2 * ((A - 1) - (A + 1) * c) / a0,
                                         ((A + 1) - (A - 1) * c - sA) / a0 };
        }();
        double sum = 0.0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            anvil::Biquad s1, s2;
            s1.setCoeffs (shelf);
            s2.setCoeffs (anvil::BiquadCoeffs::highPass (fs, 38.0, 0.5));
            const auto* x = b.getReadPointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double y = s2.process (0, s1.process (0, x[i]));
                sum += y * y;
            }
        }
        return -0.691 + 10.0 * std::log10 (std::max (sum / (double) b.getNumSamples(), 1.0e-20));
    }

    float peakDb (const Buf& b) { return juce::Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples()), -100.0f); }

    bool readWav (const juce::File& f, Buf& out, double& fs, double maxSeconds)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return false;
        fs = r->sampleRate;
        const auto len = (int) std::min ((juce::int64) (maxSeconds * fs), r->lengthInSamples);
        out.setSize (2, len);
        r->read (&out, 0, len, 0, true, true);   // mono files are duplicated to both channels
        if (r->numChannels == 1) out.copyFrom (1, 0, out, 0, 0, len);
        out.applyGain (juce::Decibels::decibelsToGain (-6.0f) / std::max (out.getMagnitude (0, len), 1.0e-6f));   // bus-ish level: peak −6 dBFS
        return true;
    }

    void writeWav (const juce::File& f, const Buf& b, double fs)
    {
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        if (os == nullptr) return;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), fs, 2, 24, {}, 0));
        if (w == nullptr) return;
        os.release();
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    void heatSweep (const Buf& in, double fs)
    {
        std::printf ("\n  Heat stage alone (K-weighted level change, dB)\n  drive |   raw   | comp law | with law\n");
        const double ref = loudness (in, fs);
        for (float d = 0.0f; d <= 20.0f; d += 2.0f)
        {
            double out[2];
            for (int pass = 0; pass < 2; ++pass)
            {
                anvil::HeatStage h;
                h.prepare (fs, 512, 2);
                h.setCompensationEnabled (pass == 1);
                h.setTargets (d);
                h.snap();
                Buf b (in);
                for (int off = 0; off < b.getNumSamples(); off += 512)
                {
                    const int n = std::min (512, b.getNumSamples() - off);
                    float* ch[] = { b.getWritePointer (0) + off, b.getWritePointer (1) + off };
                    h.process (ch, 2, n);
                }
                out[pass] = loudness (b, fs) - ref;
            }
            std::printf ("  %5.1f | %+7.2f | %+8.2f | %+7.2f\n", d, out[0], anvil::HeatStage::compensationDb (d), out[1]);
        }
    }

    void mixSweep (const Buf& in, double fs, const juce::File& outDir, const juce::String& stem)
    {
        std::printf ("\n  Mix Ready sweep, full chain (K-weighted loudness rel. input, peak dBFS, limiter engaged?)\n  mix %% | loud dB | peak dBFS\n");
        const double ref = loudness (in, fs);
        for (float mix : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
        {
            RitualAnvilAudioProcessor p;
            p.apvts.getParameter (ParameterIDs::mixReady)->setValueNotifyingHost (p.apvts.getParameter (ParameterIDs::mixReady)->convertTo0to1 (mix));
            p.prepareToPlay (fs, 512);
            const int lat = p.getLatencySamples();

            Buf b (2, in.getNumSamples() + lat);
            b.clear();
            for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, in, ch, 0, in.getNumSamples());
            juce::MidiBuffer midi;
            for (int off = 0; off < b.getNumSamples(); off += 512)
            {
                const int n = std::min (512, b.getNumSamples() - off);
                float* ch[] = { b.getWritePointer (0) + off, b.getWritePointer (1) + off };
                juce::AudioBuffer<float> view (ch, 2, n);
                p.processBlock (view, midi);
            }
            Buf aligned (2, in.getNumSamples());
            for (int ch = 0; ch < 2; ++ch) aligned.copyFrom (ch, 0, b, ch, lat, in.getNumSamples());

            const double l = loudness (aligned, fs) - ref;
            std::printf ("  %5.0f | %+7.2f | %+8.2f\n", mix, l, peakDb (aligned));

            const auto name = stem + "_mix" + juce::String ((int) mix).paddedLeft ('0', 3);
            writeWav (outDir.getChildFile (name + ".wav"), aligned, fs);
            Buf matched (aligned);
            matched.applyGain (juce::Decibels::decibelsToGain ((float) -l));   // loudness-matched to input (for fair A/B)
            writeWav (outDir.getChildFile (name + "_matched.wav"), matched, fs);
        }
        writeWav (outDir.getChildFile (stem + "_input.wav"), in, fs);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3)
    {
        std::puts ("usage: RitualAnvil_Calibrate <outDir> <drums.wav> [more.wav ...]");
        return 1;
    }
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    outDir.createDirectory();

    for (int a = 2; a < argc; ++a)
    {
        const juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[a]));
        Buf in; double fs = 48000.0;
        if (! readWav (f, in, fs, 30.0)) { std::printf ("cannot read %s\n", argv[a]); continue; }
        const auto stem = "stem" + juce::String (a - 1);
        std::printf ("\n== %s (%s, %.0f Hz, %.1f s, normalised to −6 dBFS peak) ==\n", stem.toRawUTF8(),
                     f.getFileName().toRawUTF8(), fs, in.getNumSamples() / fs);
        heatSweep (in, fs);
        mixSweep (in, fs, outDir, stem);
    }
    std::printf ("\nRenders written to %s\n", outDir.getFullPathName().toRawUTF8());
    return 0;
}
