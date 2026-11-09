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

    // Report latency from construction: some hosts read it before prepareToPlay().
    // The polyphase IIR latency depends only on the filter design, not the sample rate.
    juce::dsp::Oversampling<float> probe (2, oversamplingOrder,
                                          juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    probe.initProcessing (1);
    latencySamples = (int) std::lround (probe.getLatencyInSamples());
    setLatencySamples (latencySamples);
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
    const auto numChannels = juce::jlimit (1, rc76::kMaxChannels, getTotalNumInputChannels());
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels, oversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,    // max quality
        true);   // integer latency
    oversampling->initProcessing ((size_t) maxBlockSize);

    latencySamples = (int) std::lround (oversampling->getLatencyInSamples());
    setLatencySamples (latencySamples);

    const auto osRate = sampleRate * (double) oversampling->getOversamplingFactor();
    transformer.prepare (osRate);
    compressor.prepare (osRate);
    dcBlocker.prepare (osRate);
    classADrive.reset (osRate, 0.010);
    classADrive.setCurrentAndTargetValue (1.0f);
    classA.setDrive (1.0f);

    dryDelay.setMaximumDelayInSamples (latencySamples + 1);
    dryDelay.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) numChannels });
    dryDelay.setDelay ((float) latencySamples);
    dryBuffer.setSize (numChannels, maxBlockSize);

    inputGain.reset (sampleRate, 0.020);
    outputGain.reset (sampleRate, 0.020);
    bypassMix.reset (sampleRate, 0.010);
    inputGain.setCurrentAndTargetValue (attenToGain (inputParam->load(), inputDriveDb));
    outputGain.setCurrentAndTargetValue (attenToGain (outputParam->load(), outputMakeupDb));
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    meterGrDb.store (0.0f);
    meterOutDbfs.store (-100.0f);
}

void RitualComp76AudioProcessor::reset()
{
    if (oversampling != nullptr)
        oversampling->reset();
    transformer.reset();
    compressor.reset();
    dcBlocker.reset();
    dryDelay.reset();
}

void RitualComp76AudioProcessor::processWetOversampled (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = juce::jmin ((int) block.getNumChannels(), rc76::kMaxChannels);
    const auto numSamples  = (int) block.getNumSamples();

    float* ptrs[rc76::kMaxChannels] {};
    for (int ch = 0; ch < numChannels; ++ch)
        ptrs[ch] = block.getChannelPointer ((size_t) ch);

    // Input transformer
    for (int ch = 0; ch < numChannels; ++ch)
        for (int n = 0; n < numSamples; ++n)
            ptrs[ch][n] = transformer.process (ch, ptrs[ch][n]);

    // FET cell + feedback detector (+ FET distortion)
    lastBlockGrDb = juce::jmax (lastBlockGrDb, compressor.process (ptrs, numChannels, numSamples));

    // Class-A output amplifier (drive glides on All-mode switches), then remove DC
    for (int n = 0; n < numSamples; ++n)
    {
        if (classADrive.isSmoothing())
            classA.setDrive (classADrive.getNextValue());
        for (int ch = 0; ch < numChannels; ++ch)
            ptrs[ch][n] = dcBlocker.process (ch, classA.process (ptrs[ch][n]));
    }
}

void RitualComp76AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalSamples = buffer.getNumSamples();
    const auto numChannels  = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels(), rc76::kMaxChannels);

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, totalSamples);

    if (totalSamples == 0 || numChannels == 0 || oversampling == nullptr)
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
    classADrive.setTargetValue (settings.ratio == RatioMode::All ? allModeClassADrive : 1.0f);

    lastBlockGrDb = 0.0f;
    float* const* data = buffer.getArrayOfWritePointers();

    // Process in chunks no larger than the prepared block size (oversampler buffers are fixed)
    for (int offset = 0; offset < totalSamples; offset += maxBlockSize)
    {
        const auto numSamples = juce::jmin (maxBlockSize, totalSamples - offset);

        // ---- dry path, delayed to match the oversampling latency ----
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* dry = dryBuffer.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n)
            {
                dryDelay.pushSample (ch, data[ch][offset + n]);
                dry[n] = dryDelay.popSample (ch);
            }
        }

        const bool fullyBypassed = ! bypassMix.isSmoothing() && bypassMix.getTargetValue() >= 1.0f;
        if (fullyBypassed)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                juce::FloatVectorOperations::copy (data[ch] + offset, dryBuffer.getReadPointer (ch), numSamples);
            inputGain.skip (numSamples);
            outputGain.skip (numSamples);
            bypassMix.skip (numSamples);
            continue;
        }

        // ---- input attenuator ----
        for (int n = 0; n < numSamples; ++n)
        {
            const auto g = inputGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                data[ch][offset + n] *= g;
        }

        // ---- oversampled nonlinear core ----
        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) numChannels)
                        .getSubBlock ((size_t) offset, (size_t) numSamples);
        auto up = oversampling->processSamplesUp (sub);
        processWetOversampled (up);
        oversampling->processSamplesDown (sub);

        // ---- output attenuator + bypass crossfade ----
        for (int n = 0; n < numSamples; ++n)
        {
            const auto g = outputGain.getNextValue();
            const auto b = bypassMix.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& s = data[ch][offset + n];
                s = s * g * (1.0f - b) + dryBuffer.getSample (ch, n) * b;
            }
        }
    }

    if (bypassMix.getTargetValue() >= 1.0f && ! bypassMix.isSmoothing())
        compressor.reset();   // come back from bypass without stale gain reduction

    // ---- meters ----
    float rms = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        rms = juce::jmax (rms, buffer.getRMSLevel (ch, 0, totalSamples));

    const auto shownGr = lastBlockGrDb + (compressor.isAllMode() && lastBlockGrDb > 0.5f ? allModeMeterPinDb : 0.0f);
    atomicMax (meterGrDb, shownGr);
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
