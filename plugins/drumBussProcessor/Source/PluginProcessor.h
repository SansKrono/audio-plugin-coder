/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>

//==============================================================================
/**
*/
class drumBussProcessorAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    drumBussProcessorAudioProcessor();
    ~drumBussProcessorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // APVTS for Parameter Management
    juce::AudioProcessorValueTreeState apvts;

private:
    // Parameter Layout
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP Components
    juce::dsp::Gain<float> inputGain;
    juce::dsp::WaveShaper<float> inputClipper;
    juce::dsp::Compressor<float> compressor; // VCA (Glue)
    juce::dsp::Gain<float> makeupGain; // Auto-makeup for VCA
    juce::dsp::Compressor<float> fetCompressor; // Parallel (Monster)
    juce::dsp::WaveShaper<float> outputClipper;
    juce::dsp::Gain<float> outputGain;

    // DSP State
    double currentSampleRate = 44100.0;
    
    // Smoothed Values for Parameters
    juce::SmoothedValue<float> smoothedInputTrim;
    juce::SmoothedValue<float> smoothedMixReady;
    juce::SmoothedValue<float> smoothedOutputTrim;

    // Helper functions
    float mapMixReadyToThreshold(float mixReadyNorm);
    float mapMixReadyToMakeup(float mixReadyNorm);
    float mapMixReadyToParallelMix(float mixReadyNorm);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (drumBussProcessorAudioProcessor)
};
