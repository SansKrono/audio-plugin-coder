#pragma once

#include <juce_dsp/juce_dsp.h>

namespace anvil
{
    /** Phase-coherent 3-band split (LR4).

          low  = LP(f1) · AP(f2)     (allpass compensates the f2 split)
          mid  = HP(f1) · LP(f2)
          high = HP(f1) · HP(f2)

        low + mid + high = AP(f1) · AP(f2): magnitude-flat at any setting. */
    class BandSplitter3
    {
    public:
        void prepare (double sampleRate, int maxBlock, float f1, float f2)
        {
            fs = sampleRate;
            const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
            split1.prepare (spec);
            split2.prepare (spec);
            lowComp.prepare (spec);
            lowComp.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
            setFrequencies (f1, f2);
            reset();
        }

        void setFrequencies (float f1, float f2)
        {
            const auto maxF = (float) (fs * 0.45);
            split1.setCutoffFrequency (juce::jlimit (10.0f, maxF, f1));
            split2.setCutoffFrequency (juce::jlimit (10.0f, maxF, f2));
            lowComp.setCutoffFrequency (juce::jlimit (10.0f, maxF, f2));
        }

        void reset()
        {
            split1.reset();
            split2.reset();
            lowComp.reset();
        }

        void process (int ch, float x, float& low, float& mid, float& high) noexcept
        {
            float lo1 = 0.0f, rest = 0.0f;
            split1.processSample (ch, x, lo1, rest);
            split2.processSample (ch, rest, mid, high);
            low = lowComp.processSample (ch, lo1);
        }

    private:
        double fs = 48000.0;
        juce::dsp::LinkwitzRileyFilter<float> split1, split2, lowComp;
    };
}
