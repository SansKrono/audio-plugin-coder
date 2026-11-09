#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "ParameterIDs.hpp"
#include "MacroMapper.h"
#include "Stages.h"

class RitualAnvilAudioProcessor : public juce::AudioProcessor
{
public:
    RitualAnvilAudioProcessor();
    ~RitualAnvilAudioProcessor() override = default;

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

    //==========================================================================
    // Meter feed for the editor (message thread). Peak-since-last-call semantics.
    struct MeterSnapshot
    {
        float inDbfs, outDbfs, punchGrDb, limiterGrDb;
    };
    MeterSnapshot popMeters() noexcept;

    /** Effective (post Mix Ready) value of a slider parameter, last processed block. */
    float getEffectiveValue (int index) const noexcept { return effective[(size_t) index].load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void readParameters() noexcept;
    void processChunk (float* const* data, int numChannels, int numSamples) noexcept;

    std::array<std::atomic<float>*, anvil::NumSliderParams> rawParams {};
    juce::AudioParameterBool* bypassParam = nullptr;

    // DSP chain (architecture.md "Processing Chain")
    anvil::InputStage     input;
    anvil::TransientStage transient;
    anvil::ToneEQ         toneEq;
    anvil::PunchStage     punch;
    anvil::HeatStage      heat;
    anvil::WidthStage     width;
    anvil::PeakLimiter    limiter;

    // Bypass crossfade (no latency in 4.1.1, dry is the untouched input)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;
    juce::AudioBuffer<float> dryBuffer;
    int maxBlockSize = 512;

    std::array<std::atomic<float>, anvil::NumSliderParams> effective {};
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };    // linear peak
    std::atomic<float> meterPunchGr { 0.0f }, meterLimGr { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualAnvilAudioProcessor)
};
