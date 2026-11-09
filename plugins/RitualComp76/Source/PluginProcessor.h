#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "FetCompressor.h"
#include "ParameterIDs.hpp"

class RitualComp76AudioProcessor : public juce::AudioProcessor
{
public:
    RitualComp76AudioProcessor();
    ~RitualComp76AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** Meter values for the editor (message thread). Each call returns the peak since the last call. */
    float popGainReductionDb() noexcept { return meterGrDb.exchange (0.0f); }
    float popOutputLevelDbfs() noexcept { return meterOutDbfs.exchange (-100.0f); }

    juce::AudioProcessorValueTreeState apvts;

    // Fixed internal gain staging: attenuator 0 dB == +24 dB into the cell (≈ +48 dB total available)
    static constexpr float inputDriveDb  = 24.0f;
    static constexpr float outputMakeupDb = 24.0f;

private:
    static float attenToGain (float attenDb, float stageGainDb) noexcept;

    // Cached raw parameter pointers (lock-free reads on the audio thread)
    std::atomic<float>* inputParam   = nullptr;
    std::atomic<float>* outputParam  = nullptr;
    std::atomic<float>* attackParam  = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* ratioParam   = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    FetCompressor compressor;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain, outputGain, bypassMix;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<float> meterGrDb { 0.0f };
    std::atomic<float> meterOutDbfs { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualComp76AudioProcessor)
};
