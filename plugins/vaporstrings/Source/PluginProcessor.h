#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <atomic>
#include "ParameterIDs.hpp"
#include "dsp/Analysis.h"
#include "dsp/Synth.h"
#include "dsp/Shifter.h"
#include "dsp/Envelope.h"
#include "dsp/FxChain1.h"
#include "dsp/FxChain2.h"

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
    std::atomic<float> velocity { 0.0f };
    std::atomic<uint32_t> onsets { 0 };

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
    std::atomic<float>* pTrackHpf   = nullptr;
    std::atomic<float>* pTrackSpeed = nullptr;
    std::atomic<float>* pTrackSens  = nullptr;
    std::atomic<float>* pNotePrio   = nullptr;
    std::atomic<float>* pVelSens    = nullptr;

    // Analysis (phase 2.1.2): pitch tracker + pick onset/velocity
    vs::PitchTracker tracker;
    vs::OnsetDetector onset;
    float lastTrackHpf = -1.0f;
    float noteVelocity = 0.0f;     // velocity of the most recent pick (0..1)

    // Synth source [C] (phase 2.1.3)
    vs::SynthSource synth;
    std::atomic<float>* pGlide = nullptr;
    std::atomic<float>* pSynthLvl = nullptr;
    std::atomic<float>* pOsc1Wave = nullptr;  std::atomic<float>* pOsc1Oct = nullptr;
    std::atomic<float>* pOsc2Wave = nullptr;  std::atomic<float>* pOsc2Oct = nullptr;
    std::atomic<float>* pOsc2Det  = nullptr;  std::atomic<float>* pOscMix  = nullptr;
    std::atomic<float>* pPulseW   = nullptr;  std::atomic<float>* pUnison  = nullptr;
    std::atomic<float>* pSpread   = nullptr;  std::atomic<float>* pSubLvl  = nullptr;
    juce::SmoothedValue<float> synthLvlSm;
    float confGain = 0.0f, confCoef = 0.0f;   // fades source C in/out with tracker confidence
    juce::AudioBuffer<float> wetBuf;          // stereo wet bus (direct + octave + synth)

    // Octave layer [B] + Square-Up on [A] (phase 2.1.4)
    vs::PitchShifter shiftDown, shiftUp, shiftFree;
    vs::SquareUp squareUp;
    std::atomic<float>* pDirectSq = nullptr;
    std::atomic<float>* pOctLvl   = nullptr;
    std::atomic<float>* pOctDown  = nullptr;
    std::atomic<float>* pOctUp    = nullptr;
    std::atomic<float>* pOctShift = nullptr;
    juce::SmoothedValue<float> octLvlSm, octDownSm, octUpSm, freeMixSm;
    // The square-up oversampler delays source A by a few samples; the dry path is
    // delayed to match so Dry/Wet blends never comb-filter.
    std::array<float, 64> dryDelay {};
    int dryDelayPos = 0;

    // Synth Voice (phase 2.1.5): envelope + ladder filter on the mixed sources
    vs::VoiceEnvelope voiceEnv;
    juce::dsp::LadderFilter<float> ladder;
    std::atomic<float>* pFltType = nullptr;  std::atomic<float>* pCutoff  = nullptr;
    std::atomic<float>* pRes     = nullptr;  std::atomic<float>* pFltDrv  = nullptr;
    std::atomic<float>* pEnvAmt  = nullptr;  std::atomic<float>* pEnvMode = nullptr;
    std::atomic<float>* pAtk     = nullptr;  std::atomic<float>* pDec     = nullptr;
    std::atomic<float>* pSus     = nullptr;  std::atomic<float>* pRel     = nullptr;
    juce::AudioBuffer<float> envBuf;          // per-sample envelope level for the filter
    int lastFltType = -1;
    static constexpr int kFilterChunk = 32;   // cutoff modulation rate (samples)

    // FX chain I (phase 2.1.6): Slowdown → Drive → Ensemble → Wobble/Age
    vs::Slowdown fxSlow;   vs::Bypass bpSlow;
    vs::Drive    fxDrive;  vs::Bypass bpDrive;
    vs::Ensemble fxEns;    vs::Bypass bpEns;
    vs::Wobble   fxWob;    vs::Bypass bpWob;
    std::atomic<float>* pSlowOn  = nullptr; std::atomic<float>* pSlowPitch = nullptr; std::atomic<float>* pSlowDrift = nullptr;
    std::atomic<float>* pDriveOn = nullptr; std::atomic<float>* pDriveMode = nullptr;
    std::atomic<float>* pDriveAmt = nullptr; std::atomic<float>* pDriveTone = nullptr;
    std::atomic<float>* pEnsOn   = nullptr; std::atomic<float>* pEnsMode = nullptr; std::atomic<float>* pEnsRate = nullptr;
    std::atomic<float>* pEnsDepth = nullptr; std::atomic<float>* pEnsMix = nullptr;
    std::atomic<float>* pWobOn   = nullptr; std::atomic<float>* pWobWow = nullptr; std::atomic<float>* pWobFlutter = nullptr;
    std::atomic<float>* pWobAge  = nullptr; std::atomic<float>* pWobDrop = nullptr;

    // FX chain II (phase 2.1.7): Haze → Echo, send-style with tail spillover
    vs::Haze fxHaze;  vs::Smooth hazeMix, hazeOnS;  bool hazeActive = false;
    vs::Echo fxEcho;  vs::Smooth echoMix, echoOnS;  bool echoActive = false;
    std::atomic<float>* pHazeOn = nullptr;  std::atomic<float>* pHazeSize = nullptr; std::atomic<float>* pHazeDecay = nullptr;
    std::atomic<float>* pHazePre = nullptr; std::atomic<float>* pHazeMod = nullptr;  std::atomic<float>* pHazeDamp = nullptr;
    std::atomic<float>* pHazeRev = nullptr; std::atomic<float>* pHazeFrz = nullptr;  std::atomic<float>* pHazeMix = nullptr;
    std::atomic<float>* pEchoOn = nullptr;  std::atomic<float>* pEchoSync = nullptr; std::atomic<float>* pEchoTime = nullptr;
    std::atomic<float>* pEchoDiv = nullptr; std::atomic<float>* pEchoFb = nullptr;   std::atomic<float>* pEchoTone = nullptr;
    std::atomic<float>* pEchoMix = nullptr;
    double hostBpm = 120.0;

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
