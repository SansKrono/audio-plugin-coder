#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    constexpr int kMaxChannels = 2;

    void atomicMax (std::atomic<float>& target, float value) noexcept
    {
        auto prev = target.load (std::memory_order_relaxed);
        while (value > prev && ! target.compare_exchange_weak (prev, value, std::memory_order_relaxed)) {}
    }

    float blockPeak (const float* const* data, int numChannels, int numSamples) noexcept
    {
        float pk = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto r = juce::FloatVectorOperations::findMinAndMax (data[ch], numSamples);
            pk = juce::jmax (pk, std::abs (r.getStart()), std::abs (r.getEnd()));
        }
        return pk;
    }

    juce::String valueToText (float v, int decimals, const char* unit)
    {
        return juce::String (v, decimals) + (unit[0] != 0 ? juce::String (" ") + unit : juce::String());
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout RitualAnvilAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < anvil::NumSliderParams; ++i)
    {
        const auto& s = anvil::sliderSpecs[(size_t) i];
        NormalisableRange<float> range { s.min, s.max, 0.01f };
        if (i == anvil::Hpf)
            range.setSkewForCentre (std::sqrt (s.min * s.max));   // log-like taper, 44.7 Hz at centre

        const int decimals = (i == anvil::Hpf || i == anvil::MixReady || i == anvil::Punch || i == anvil::Width) ? 0
                           : (i == anvil::Ceiling) ? 2 : 1;
        const auto* unit = s.label;

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { s.id, 1 }, s.name, range, s.def,
            AudioParameterFloatAttributes().withLabel (s.label)
                                           .withStringFromValueFunction ([decimals, unit] (float v, int) { return valueToText (v, decimals, unit); })));
    }

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));
    return layout;
}

//==============================================================================
RitualAnvilAudioProcessor::RitualAnvilAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RitualAnvil", createParameterLayout())
{
    for (int i = 0; i < anvil::NumSliderParams; ++i)
    {
        rawParams[(size_t) i] = apvts.getRawParameterValue (anvil::sliderSpecs[(size_t) i].id);
        jassert (rawParams[(size_t) i] != nullptr);
        effective[(size_t) i].store (anvil::sliderSpecs[(size_t) i].def);
    }
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParameterIDs::bypass));
    jassert (bypassParam != nullptr);
}

bool RitualAnvilAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void RitualAnvilAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = juce::jlimit (1, kMaxChannels, getTotalNumInputChannels());
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    input.prepare (sampleRate);
    transient.prepare (sampleRate, maxBlockSize);
    toneEq.prepare (sampleRate);
    punch.prepare (sampleRate);
    heat.prepare (sampleRate, maxBlockSize);
    width.prepare (sampleRate);
    limiter.prepare (sampleRate);

    dryBuffer.setSize (numChannels, maxBlockSize);
    bypassMix.reset (sampleRate, 0.010);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    setLatencySamples (0);

    // Start smoothers at the current parameter values (no ramp from defaults on load)
    readParameters();
    input.snap();
    transient.snap();
    toneEq.snap();
    punch.snap();
    heat.snap();
    width.snap();
    limiter.snap();

    meterIn.store (0.0f);
    meterOut.store (0.0f);
    meterPunchGr.store (0.0f);
    meterLimGr.store (0.0f);
}

void RitualAnvilAudioProcessor::reset()
{
    input.reset();
    transient.reset();
    toneEq.reset();
    punch.reset();
    heat.reset();
    width.reset();
    limiter.reset();
}

//==============================================================================
void RitualAnvilAudioProcessor::readParameters() noexcept
{
    anvil::AnvilParams manual {};
    for (int i = 0; i < anvil::NumSliderParams; ++i)
    {
        const auto& s = anvil::sliderSpecs[(size_t) i];
        const float v = rawParams[(size_t) i]->load();
        manual[(size_t) i] = std::isfinite (v) ? juce::jlimit (s.min, s.max, v) : s.def;
    }

    const auto e = anvil::applyMacro (manual);
    for (int i = 0; i < anvil::NumSliderParams; ++i)
        effective[(size_t) i].store (e[(size_t) i], std::memory_order_relaxed);

    input.setTargets (e[anvil::InputGain], e[anvil::Hpf]);
    transient.setTargets (e[anvil::TransientAttack], e[anvil::TransientSustain]);
    const float eq[anvil::ToneEQ::numBands] = { e[anvil::EqKick], e[anvil::EqBox], e[anvil::EqSnare],
                                                e[anvil::EqPresence], e[anvil::EqAir] };
    toneEq.setTargets (eq);
    punch.setTargets (e[anvil::Punch] / 100.0f * anvil::fixed::punchMaxBlend);
    heat.setTargets (e[anvil::Heat]);
    width.setTargets (e[anvil::Width]);
    limiter.setTargets (e[anvil::Ceiling]);

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
}

void RitualAnvilAudioProcessor::processChunk (float* const* data, int numChannels, int numSamples) noexcept
{
    input.process (data, numChannels, numSamples);
    transient.process (data, numChannels, numSamples);
    toneEq.process (data, numChannels, numSamples);
    atomicMax (meterPunchGr, punch.process (data, numChannels, numSamples));
    heat.process (data, numChannels, numSamples);
    width.process (data, numChannels, numSamples);
    atomicMax (meterLimGr, limiter.process (data, numChannels, numSamples));
}

void RitualAnvilAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalSamples = buffer.getNumSamples();
    const auto numChannels  = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels(), kMaxChannels);

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, totalSamples);

    if (totalSamples == 0 || numChannels == 0)
        return;

    readParameters();

    float* const* data = buffer.getArrayOfWritePointers();
    atomicMax (meterIn, blockPeak (data, numChannels, totalSamples));

    // Chunks no larger than the prepared size (dry buffer is fixed)
    for (int offset = 0; offset < totalSamples; offset += maxBlockSize)
    {
        const auto n = juce::jmin (maxBlockSize, totalSamples - offset);
        float* chunk[kMaxChannels] {};
        for (int ch = 0; ch < numChannels; ++ch)
            chunk[ch] = data[ch] + offset;

        const bool fullyBypassed = ! bypassMix.isSmoothing() && bypassMix.getTargetValue() >= 1.0f;
        if (fullyBypassed)
        {
            bypassMix.skip (n);
            continue;   // dry passes through untouched
        }

        const bool crossfading = bypassMix.isSmoothing();
        if (crossfading)
            for (int ch = 0; ch < numChannels; ++ch)
                juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (ch), chunk[ch], n);

        processChunk (chunk, numChannels, n);

        if (crossfading)
        {
            for (int i = 0; i < n; ++i)
            {
                const float m = bypassMix.getNextValue();
                for (int ch = 0; ch < numChannels; ++ch)
                    chunk[ch][i] = chunk[ch][i] * (1.0f - m) + dryBuffer.getSample (ch, i) * m;
            }
        }
        else
        {
            bypassMix.skip (n);
        }
    }

    atomicMax (meterOut, blockPeak (data, numChannels, totalSamples));
}

RitualAnvilAudioProcessor::MeterSnapshot RitualAnvilAudioProcessor::popMeters() noexcept
{
    const auto toDb = [] (float g) { return g > 1.0e-5f ? juce::Decibels::gainToDecibels (g) : -100.0f; };
    return { toDb (meterIn.exchange (0.0f)),
             toDb (meterOut.exchange (0.0f)),
             meterPunchGr.exchange (0.0f),
             meterLimGr.exchange (0.0f) };
}

//==============================================================================
void RitualAnvilAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RitualAnvilAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RitualAnvilAudioProcessor::createEditor()
{
    return new RitualAnvilAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RitualAnvilAudioProcessor();
}
