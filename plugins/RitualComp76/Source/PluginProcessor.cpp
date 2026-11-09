#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    void atomicMax (std::atomic<float>& target, float value) noexcept
    {
        auto prev = target.load (std::memory_order_relaxed);
        while (value > prev && ! target.compare_exchange_weak (prev, value, std::memory_order_relaxed)) {}
    }

    juce::String attenToText (float v, int)
    {
        return v < ParameterRanges::muteBelowDb ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e"))
                                                : juce::String (juce::roundToInt (v)) + " dB";
    }

    float textToAtten (const juce::String& text)
    {
        const auto t = text.trim();
        if (t.isEmpty() || t.startsWithIgnoreCase ("inf") || t == juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")))
            return ParameterRanges::attenMinDb;
        return juce::jlimit (ParameterRanges::attenMinDb, ParameterRanges::attenMaxDb, t.getFloatValue());
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout RitualComp76AudioProcessor::createParameterLayout()
{
    using namespace juce;
    using namespace ParameterRanges;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const NormalisableRange<float> attenRange { attenMinDb, attenMaxDb, 0.1f };
    const NormalisableRange<float> stepRange  { stepMin, stepMax, 0.01f };

    auto attenAttrs = AudioParameterFloatAttributes().withLabel ("dB")
                          .withStringFromValueFunction (attenToText)
                          .withValueFromStringFunction (textToAtten);

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParameterIDs::input, 1 }, "Input",
                                                       attenRange, attenDefault, attenAttrs));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParameterIDs::output, 1 }, "Output",
                                                       attenRange, attenDefault, attenAttrs));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParameterIDs::attack, 1 }, "Attack", stepRange, attackDefault,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return String (v, 1) + " (" + String (juce::roundToInt (FetCompressor::attackSecondsFromStep (v) * 1.0e6f)) + " us)";
        })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParameterIDs::release, 1 }, "Release", stepRange, releaseDefault,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
            return String (v, 1) + " (" + String (juce::roundToInt (FetCompressor::releaseSecondsFromStep (v) * 1.0e3f)) + " ms)";
        })));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParameterIDs::ratio, 1 }, "Ratio",
                                                        StringArray { "Out", "4:1", "8:1", "12:1", "20:1", "All" },
                                                        (int) RatioMode::R4));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParameterIDs::meterMode, 1 }, "Meter",
                                                        StringArray { "GR", "+8", "+4", "Off" },
                                                        (int) MeterMode::GR));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
RitualComp76AudioProcessor::RitualComp76AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RitualComp76", createParameterLayout())
{
    inputParam   = apvts.getRawParameterValue (ParameterIDs::input);
    outputParam  = apvts.getRawParameterValue (ParameterIDs::output);
    attackParam  = apvts.getRawParameterValue (ParameterIDs::attack);
    releaseParam = apvts.getRawParameterValue (ParameterIDs::release);
    ratioParam   = apvts.getRawParameterValue (ParameterIDs::ratio);
    bypassParam  = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParameterIDs::bypass));
    jassert (inputParam && outputParam && attackParam && releaseParam && ratioParam && bypassParam);
}

bool RitualComp76AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

float RitualComp76AudioProcessor::attenToGain (float attenDb, float stageGainDb) noexcept
{
    if (! std::isfinite (attenDb) || attenDb < ParameterRanges::muteBelowDb)
        return 0.0f;
    return juce::Decibels::decibelsToGain (juce::jlimit (ParameterRanges::attenMinDb, ParameterRanges::attenMaxDb, attenDb) + stageGainDb);
}

//==============================================================================
void RitualComp76AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    compressor.prepare (sampleRate);

    inputGain.reset (sampleRate, 0.020);
    outputGain.reset (sampleRate, 0.020);
    bypassMix.reset (sampleRate, 0.010);
    inputGain.setCurrentAndTargetValue (attenToGain (inputParam->load(), inputDriveDb));
    outputGain.setCurrentAndTargetValue (attenToGain (outputParam->load(), outputMakeupDb));
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    dryBuffer.setSize (juce::jmax (2, getTotalNumInputChannels()), juce::jmax (1, samplesPerBlock));
    meterGrDb.store (0.0f);
    meterOutDbfs.store (-100.0f);
}

void RitualComp76AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0 || numChannels == 0)
        return;

    // ---- parameters (once per block) ----
    inputGain.setTargetValue (attenToGain (inputParam->load(), inputDriveDb));
    outputGain.setTargetValue (attenToGain (outputParam->load(), outputMakeupDb));
    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    FetCompressor::Settings settings;
    settings.attackStep  = juce::jlimit (ParameterRanges::stepMin, ParameterRanges::stepMax, attackParam->load());
    settings.releaseStep = juce::jlimit (ParameterRanges::stepMin, ParameterRanges::stepMax, releaseParam->load());
    settings.ratio       = (RatioMode) juce::jlimit (0, (int) RatioMode::All, juce::roundToInt (ratioParam->load()));
    compressor.setSettings (settings);

    const bool fullyBypassed = ! bypassMix.isSmoothing() && bypassMix.getTargetValue() >= 1.0f;
    if (fullyBypassed)
    {
        compressor.reset();
        inputGain.skip (numSamples);
        outputGain.skip (numSamples);
        atomicMax (meterOutDbfs, juce::Decibels::gainToDecibels (buffer.getRMSLevel (0, 0, numSamples), -100.0f));
        return;
    }

    // ---- keep dry copy for click-free bypass crossfade ----
    const bool needDry = bypassMix.isSmoothing() || bypassMix.getTargetValue() > 0.0f;
    if (needDry)
    {
        if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() < numChannels)
            dryBuffer.setSize (numChannels, numSamples, false, false, true);   // only if host exceeds prepared size
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    auto* const* data = buffer.getArrayOfWritePointers();

    // ---- input attenuator ----
    for (int n = 0; n < numSamples; ++n)
    {
        const auto g = inputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            data[ch][n] *= g;
    }

    // ---- FET cell + feedback detector ----
    const auto blockGr = compressor.process (data, numChannels, numSamples);

    // ---- output attenuator ----
    for (int n = 0; n < numSamples; ++n)
    {
        const auto g = outputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            data[ch][n] *= g;
    }

    // ---- bypass crossfade ----
    if (needDry)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const auto b = bypassMix.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                data[ch][n] = data[ch][n] * (1.0f - b) + dryBuffer.getSample (ch, n) * b;
        }
    }
    else
    {
        bypassMix.skip (numSamples);
    }

    // ---- meters ----
    float rms = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        rms = juce::jmax (rms, buffer.getRMSLevel (ch, 0, numSamples));

    atomicMax (meterGrDb, blockGr);
    atomicMax (meterOutDbfs, juce::Decibels::gainToDecibels (rms, -100.0f));
}

//==============================================================================
juce::AudioProcessorEditor* RitualComp76AudioProcessor::createEditor()
{
    return new RitualComp76AudioProcessorEditor (*this);
}

void RitualComp76AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RitualComp76AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RitualComp76AudioProcessor();
}
