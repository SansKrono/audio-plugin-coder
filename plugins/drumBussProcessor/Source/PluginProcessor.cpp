/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
drumBussProcessorAudioProcessor::drumBussProcessorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

drumBussProcessorAudioProcessor::~drumBussProcessorAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout drumBussProcessorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Input Trim (-24 to +24 dB)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "input_trim", "Input Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    // Mix Ready (0 to 100 %)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix_ready", "Mix Ready",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    // Output Trim (-24 to +24 dB)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "output_trim", "Output Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    // Bypass
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

    return layout;
}

//==============================================================================
const juce::String drumBussProcessorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool drumBussProcessorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool drumBussProcessorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool drumBussProcessorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double drumBussProcessorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int drumBussProcessorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int drumBussProcessorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void drumBussProcessorAudioProcessor::setCurrentProgram (int /*index*/)
{
}

const juce::String drumBussProcessorAudioProcessor::getProgramName (int /*index*/)
{
    return {};
}

void drumBussProcessorAudioProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

//==============================================================================
void drumBussProcessorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock; // Cast to fix sign conversion warning
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels(); // Cast to fix sign conversion warning

    inputGain.prepare(spec);
    inputGain.setRampDurationSeconds(0.02);

    outputGain.prepare(spec);
    outputGain.setRampDurationSeconds(0.02);

    // Initialize Clippers (Hard Safety Clip)
    auto clipperFn = [] (float x) { return juce::jlimit(-1.0f, 1.0f, x); };
    
    inputClipper.prepare(spec);
    inputClipper.functionToUse = clipperFn;
    
    outputClipper.prepare(spec);
    outputClipper.functionToUse = clipperFn;

    // Initialize VCA Compressor (Glue)
    compressor.prepare(spec);
    compressor.setRatio(4.0f);
    compressor.setAttack(30.0f);
    compressor.setRelease(100.0f);

    makeupGain.prepare(spec);
    makeupGain.setRampDurationSeconds(0.02); // Smooth makeup changes

    // Initialize FET Compressor (Monster) - Fixed aggressive settings
    fetCompressor.prepare(spec);
    fetCompressor.setRatio(20.0f); // All buttons in style
    fetCompressor.setAttack(0.1f); // Fast attack
    fetCompressor.setRelease(50.0f);
    fetCompressor.setThreshold(-20.0f); // Fixed threshold, driven by gain

    // Initialize Smoothing
    smoothedInputTrim.reset(sampleRate, 0.02);
    smoothedMixReady.reset(sampleRate, 0.02);
    smoothedOutputTrim.reset(sampleRate, 0.02);
}

void drumBussProcessorAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool drumBussProcessorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

// Helpers for Macro Mapping
float drumBussProcessorAudioProcessor::mapMixReadyToThreshold(float mixReadyNorm)
{
    // Map 0-1 to -5dB to -30dB (Softer to harder compression)
    return juce::jmap(mixReadyNorm, -5.0f, -30.0f);
}

float drumBussProcessorAudioProcessor::mapMixReadyToMakeup(float mixReadyNorm)
{
    // Auto makeup gain: 0 to 6dB
    return juce::jmap(mixReadyNorm, 0.0f, 6.0f);
}

void drumBussProcessorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Check Bypass
    bool bypass = *apvts.getRawParameterValue("bypass") > 0.5f;
    if (bypass) return;

    // Update Parameters
    float targetInputTrim = *apvts.getRawParameterValue("input_trim");
    float targetMixReady = *apvts.getRawParameterValue("mix_ready"); // 0-100
    float targetOutputTrim = *apvts.getRawParameterValue("output_trim");

    smoothedInputTrim.setTargetValue(targetInputTrim);
    smoothedMixReady.setTargetValue(targetMixReady / 100.0f); // Normalize to 0-1
    smoothedOutputTrim.setTargetValue(targetOutputTrim);

    // Processing Loop (Block or Sample)
    // For specific macro curves, we might process sample-by-sample or in small blocks 
    // to modulate parameters smoothy. Since we use SmoothedValue for the main knobs, 
    // let's do block processing for the compressors but update their parameters per block for now 
    // (optimization: don't call setThreshold every sample unless needed).
    
    // 1. Input Gain
    float currentInputTrim = smoothedInputTrim.getNextValue(); 
    juce::ignoreUnused(currentInputTrim); // Use it if we need per-sample smoothing later
    // Actually, let's trust gain's internal ramp for inputGain, but we need to feed it target.
    inputGain.setGainDecibels(targetInputTrim); 

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    inputGain.process(context);

    // Input Clipper
    inputClipper.process(context);

    // 2. Mix Ready Macro Mapping
    // Grab normalized mix ready
    // float mixReadyStart = smoothedMixReady.getCurrentValue(); // Unused
    // float mixReadyTarget = smoothedMixReady.getTargetValue(); // Unused
    
    // If modulation is fast, we need sample accuarcy, but for a macro knob, block rate is usually fine parameter update.
    float mixReady = smoothedMixReady.getNextValue(); 

    // VCA Compressor Control
    float thresh = mapMixReadyToThreshold(mixReady);
    float ratio = 4.0f; // Fixed glue ratio
    float makeup = mapMixReadyToMakeup(mixReady);

    compressor.setThreshold(thresh);
    compressor.setRatio(ratio);
    
    // Apply VCA
    compressor.process(context);

    // Apply Makeup Gain
    makeupGain.setGainDecibels(makeup);
    makeupGain.process(context);
    
    // Output Clipper
    outputClipper.process(context);
    
    // 3. Output Gain
    outputGain.setGainDecibels(targetOutputTrim);
    outputGain.process(context);
}

//==============================================================================
bool drumBussProcessorAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* drumBussProcessorAudioProcessor::createEditor()
{
    return new drumBussProcessorAudioProcessorEditor (*this);
}

//==============================================================================
void drumBussProcessorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Store parameters
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void drumBussProcessorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore parameters
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new drumBussProcessorAudioProcessor();
}
