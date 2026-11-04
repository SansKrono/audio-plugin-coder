#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <atomic>
#include "ParameterIDs.hpp"

//==============================================================================
/** Live values the editor streams to the WebView (written on the audio thread,
    read/reset on the message thread). Peaks use max-hold until consumed. */
struct VaporMeters
{
    std::atomic<float> inPeak  { 0.0f };
    std::atomic<float> outPeak { 0.0f };
    std::atomic<float> env     { 0.0f };
    std::atomic<float> noteHz  { 0.0f };  // 0 = unvoiced (tracker arrives in phase 2.1.2)
    std::atomic<float> conf    { 0.0f };
    std::atomic<bool>  gate    { false };

    static void raise (std::atomic<float>& a, float v) noexcept
    {
        auto cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
};

//==============================================================================
class VaporStringsAudioProcessor  : public juce::AudioProcessor
{
public:
    VaporStringsAudioProcessor();
    ~VaporStringsAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; } // Haze decay ceiling

    //==============================================================================
    int getNumPrograms() override                 { return ParamTable::numPresets; }
    int getCurrentProgram() override              { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    VaporMeters meters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //==============================================================================
    // Parameter handles (raw atomics, read once per block)
    std::atomic<float>* pInGain     = nullptr;
    std::atomic<float>* pGateThresh = nullptr;
    std::atomic<float>* pDirectLvl  = nullptr;
    std::atomic<float>* pWidth      = nullptr;
    std::atomic<float>* pDryWet     = nullptr;
    std::atomic<float>* pOutGain    = nullptr;

    // Smoothed controls
    juce::SmoothedValue<float> inGainSm, directSm, outGainSm, dryWetSm, widthSm;

    // Input stage: noise gate (peak detector + gain ramp, with hysteresis)
    float gateDetector = 0.0f, gateGain = 0.0f;
    float gateAtkCoef = 0.0f, gateRelCoef = 0.0f, gateRampStep = 0.0f;
    bool  gateOpen = false;

    // Envelope follower (2 ms / 120 ms) - meter + future amp/filter env
    float envState = 0.0f, envAtkCoef = 0.0f, envRelCoef = 0.0f;

    // Scratch (allocated in prepareToPlay)
    juce::AudioBuffer<float> monoBuf, dryBuf;

    int currentPreset = 0;

    static float levelDbToGain (float db) noexcept { return db <= -59.95f ? 0.0f : juce::Decibels::decibelsToGain (db); }
    static float softClip (float x) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporStringsAudioProcessor)
};
