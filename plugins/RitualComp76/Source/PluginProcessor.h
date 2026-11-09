#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "FetCompressor.h"
#include "Saturation.h"
#include "ParameterIDs.hpp"

class RitualComp76AudioProcessor : public juce::AudioProcessor
{
public:
    RitualComp76AudioProcessor();
    ~RitualComp76AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
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

    // Nonlinear core runs 4x oversampled (polyphase IIR, integer latency)
    static constexpr size_t oversamplingOrder = 2;   // 2^2 = 4x
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int latencySamples = 0;
    int maxBlockSize = 512;
    float lastBlockGrDb = 0.0f;

    rc76::InputTransformer transformer;
    FetCompressor compressor;
    rc76::ClassAStage classA;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> classADrive;   // runs at the oversampled rate
    rc76::DcBlocker dcBlocker;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain, outputGain, bypassMix;

    // Dry path delayed by the oversampling latency so bypass crossfades stay phase-aligned
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay;
    juce::AudioBuffer<float> dryBuffer;

    // All mode: the original's meter pins hard left; show extra GR on the meter only
    static constexpr float allModeMeterPinDb = 8.0f;
    static constexpr float allModeClassADrive = 1.8f;

    void processWetOversampled (juce::dsp::AudioBlock<float>& block) noexcept;

    std::atomic<float> meterGrDb { 0.0f };
    std::atomic<float> meterOutDbfs { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualComp76AudioProcessor)
};
