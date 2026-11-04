#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameter layout — built from the generated table (Source/ui/params.json)
juce::AudioProcessorValueTreeState::ParameterLayout VaporStringsAudioProcessor::createParameterLayout()
{
    using namespace ParamTable;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& d : defs)
    {
        const juce::ParameterID pid { d.id, 1 };

        switch (d.type)
        {
            case Type::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
                break;

            case Type::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
                break;

            case Type::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.min, (int) d.max, (int) d.def));
                break;

            case Type::Float:
            {
                juce::NormalisableRange<float> range (d.min, d.max, d.step);
                if (d.centre != 0.0f)
                    range.setSkewForCentre (d.centre);
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, range, d.def));
                break;
            }
        }
    }

    return layout;
}

//==============================================================================
VaporStringsAudioProcessor::VaporStringsAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VaporStrings", createParameterLayout())
{
    pInGain     = apvts.getRawParameterValue (ParameterIDs::in_gain);
    pGateThresh = apvts.getRawParameterValue (ParameterIDs::gate_thresh);
    pDirectLvl  = apvts.getRawParameterValue (ParameterIDs::src_direct_lvl);
    pWidth      = apvts.getRawParameterValue (ParameterIDs::width);
    pDryWet     = apvts.getRawParameterValue (ParameterIDs::dry_wet);
    pOutGain    = apvts.getRawParameterValue (ParameterIDs::out_gain);

    jassert (apvts.state.isValid());
    jassert (getParameters().size() == ParamTable::numParams);
}

bool VaporStringsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void VaporStringsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto sr = (float) sampleRate;
    auto coef = [sr] (float ms) { return std::exp (-1.0f / (0.001f * ms * sr)); };

    gateAtkCoef  = coef (1.0f);
    gateRelCoef  = coef (80.0f);
    gateRampStep = 1.0f / (0.005f * sr);     // 5 ms open/close ramp
    envAtkCoef   = coef (2.0f);
    envRelCoef   = coef (120.0f);

    inGainSm .reset (sampleRate, 0.02);
    directSm .reset (sampleRate, 0.02);
    outGainSm.reset (sampleRate, 0.02);
    dryWetSm .reset (sampleRate, 0.03);
    widthSm  .reset (sampleRate, 0.03);

    inGainSm .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    directSm .setCurrentAndTargetValue (levelDbToGain (pDirectLvl->load()));
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    dryWetSm .setCurrentAndTargetValue (pDryWet->load());
    widthSm  .setCurrentAndTargetValue (pWidth->load());

    monoBuf.setSize (1, samplesPerBlock, false, true, false);
    dryBuf .setSize (1, samplesPerBlock, false, true, false);

    gateDetector = gateGain = envState = 0.0f;
    gateOpen = false;
}

float VaporStringsAudioProcessor::softClip (float x) noexcept
{
    // Transparent below 0.8, soft knee up to a hard ceiling of 1.0
    const float a = std::abs (x);
    if (a <= 0.8f)
        return x;
    return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
}

void VaporStringsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    // Hosts may exceed the prepared block size; grow scratch without reallocating each block.
    if (monoBuf.getNumSamples() < numSamples)
    {
        monoBuf.setSize (1, numSamples, false, false, true);
        dryBuf .setSize (1, numSamples, false, false, true);
    }

    const int numIn  = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int numOut = buffer.getNumChannels();

    // ---- read parameters (once per block) --------------------------------------
    inGainSm .setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    directSm .setTargetValue (levelDbToGain (pDirectLvl->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    dryWetSm .setTargetValue (juce::jlimit (0.0f, 1.0f, pDryWet->load()));
    widthSm  .setTargetValue (juce::jlimit (0.0f, 2.0f, pWidth->load()));

    const float openThresh  = juce::Decibels::decibelsToGain (pGateThresh->load());
    const float closeThresh = openThresh * 0.7079f;                 // 3 dB hysteresis

    // ---- input: mono sum + input gain -------------------------------------------
    auto* mono = monoBuf.getWritePointer (0);
    auto* dry  = dryBuf.getWritePointer (0);

    if (numIn <= 0)
        juce::FloatVectorOperations::clear (mono, numSamples);
    else
    {
        juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0), numSamples);
        for (int ch = 1; ch < numIn; ++ch)
            juce::FloatVectorOperations::add (mono, buffer.getReadPointer (ch), numSamples);
        if (numIn > 1)
            juce::FloatVectorOperations::multiply (mono, 1.0f / (float) numIn, numSamples);
    }

    float inPk = 0.0f, outPk = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float x = mono[i] * inGainSm.getNextValue();
        dry[i] = x;
        const float ax = std::abs (x);
        inPk = juce::jmax (inPk, ax);

        // noise gate (also the note gate for the tracker in 2.1.2)
        gateDetector = ax > gateDetector ? gateAtkCoef * (gateDetector - ax) + ax
                                         : gateRelCoef * (gateDetector - ax) + ax;
        if (! gateOpen && gateDetector > openThresh)        gateOpen = true;
        else if (gateOpen && gateDetector < closeThresh)    gateOpen = false;
        gateGain = gateOpen ? juce::jmin (1.0f, gateGain + gateRampStep)
                            : juce::jmax (0.0f, gateGain - gateRampStep);

        const float gated = x * gateGain;

        // envelope follower
        const float ag = std::abs (gated);
        envState = ag > envState ? envAtkCoef * (envState - ag) + ag
                                 : envRelCoef * (envState - ag) + ag;

        // [A] Direct source — the only sound source in phase 2.1.1
        mono[i] = gated * directSm.getNextValue();
    }

    // ---- output: dry/wet, gain, safety clip --------------------------------------
    auto* outL = buffer.getWritePointer (0);
    auto* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = dryWetSm.getNextValue();
        const float gDry = std::cos (mix * juce::MathConstants<float>::halfPi);
        const float gWet = std::sin (mix * juce::MathConstants<float>::halfPi);
        const float g = outGainSm.getNextValue();
        widthSm.getNextValue(); // processed chain is still mono; width acts once stereo sources exist

        const float y = softClip ((dry[i] * gDry + mono[i] * gWet) * g);
        outPk = juce::jmax (outPk, std::abs (y));
        outL[i] = y;
        if (outR != nullptr) outR[i] = y;
    }

    for (int ch = 2; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    VaporMeters::raise (meters.inPeak, inPk);
    VaporMeters::raise (meters.outPeak, outPk);
    meters.env.store (envState, std::memory_order_relaxed);
    meters.gate.store (gateOpen, std::memory_order_relaxed);
}

//==============================================================================
void VaporStringsAudioProcessor::setCurrentProgram (int index)
{
    using namespace ParamTable;
    if (! juce::isPositiveAndBelow (index, numPresets))
        return;

    currentPreset = index;
    const auto& preset = presets[index];

    for (const auto& d : defs)
    {
        float value = d.def;
        for (int i = 0; i < preset.numValues; ++i)
            if (preset.values[i].id != nullptr && std::strcmp (preset.values[i].id, d.id) == 0)
                value = preset.values[i].value;

        if (auto* p = apvts.getParameter (d.id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    apvts.state.setProperty ("preset", index, nullptr);
}

const juce::String VaporStringsAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, ParamTable::numPresets) ? juce::String (ParamTable::presets[index].name)
                                                                    : juce::String();
}

//==============================================================================
void VaporStringsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VaporStringsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, ParamTable::numPresets - 1, (int) apvts.state.getProperty ("preset", 0));
        }
}

//==============================================================================
juce::AudioProcessorEditor* VaporStringsAudioProcessor::createEditor()
{
    return new VaporStringsAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VaporStringsAudioProcessor();
}
