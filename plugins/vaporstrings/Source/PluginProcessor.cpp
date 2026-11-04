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
    pTrackHpf   = apvts.getRawParameterValue (ParameterIDs::track_hpf);
    pTrackSpeed = apvts.getRawParameterValue (ParameterIDs::track_speed);
    pTrackSens  = apvts.getRawParameterValue (ParameterIDs::track_sens);
    pNotePrio   = apvts.getRawParameterValue (ParameterIDs::note_priority);
    pVelSens    = apvts.getRawParameterValue (ParameterIDs::vel_sens);

    pGlide    = apvts.getRawParameterValue (ParameterIDs::glide);
    pSynthLvl = apvts.getRawParameterValue (ParameterIDs::src_synth_lvl);
    pOsc1Wave = apvts.getRawParameterValue (ParameterIDs::osc1_wave);
    pOsc1Oct  = apvts.getRawParameterValue (ParameterIDs::osc1_oct);
    pOsc2Wave = apvts.getRawParameterValue (ParameterIDs::osc2_wave);
    pOsc2Oct  = apvts.getRawParameterValue (ParameterIDs::osc2_oct);
    pOsc2Det  = apvts.getRawParameterValue (ParameterIDs::osc2_detune);
    pOscMix   = apvts.getRawParameterValue (ParameterIDs::osc_mix);
    pPulseW   = apvts.getRawParameterValue (ParameterIDs::pulse_width);
    pUnison   = apvts.getRawParameterValue (ParameterIDs::unison);
    pSpread   = apvts.getRawParameterValue (ParameterIDs::unison_spread);
    pSubLvl   = apvts.getRawParameterValue (ParameterIDs::sub_lvl);

    pDirectSq = apvts.getRawParameterValue (ParameterIDs::direct_square);
    pOctLvl   = apvts.getRawParameterValue (ParameterIDs::src_oct_lvl);
    pOctDown  = apvts.getRawParameterValue (ParameterIDs::oct_down);
    pOctUp    = apvts.getRawParameterValue (ParameterIDs::oct_up);
    pOctShift = apvts.getRawParameterValue (ParameterIDs::oct_shift);

    pFltType  = apvts.getRawParameterValue (ParameterIDs::flt_type);
    pCutoff   = apvts.getRawParameterValue (ParameterIDs::flt_cutoff);
    pRes      = apvts.getRawParameterValue (ParameterIDs::flt_res);
    pFltDrv   = apvts.getRawParameterValue (ParameterIDs::flt_drive);
    pEnvAmt   = apvts.getRawParameterValue (ParameterIDs::flt_env_amt);
    pEnvMode  = apvts.getRawParameterValue (ParameterIDs::env_mode);
    pAtk      = apvts.getRawParameterValue (ParameterIDs::env_attack);
    pDec      = apvts.getRawParameterValue (ParameterIDs::env_decay);
    pSus      = apvts.getRawParameterValue (ParameterIDs::env_sustain);
    pRel      = apvts.getRawParameterValue (ParameterIDs::env_release);

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pSlowOn = P (ParameterIDs::slow_on);   pSlowPitch = P (ParameterIDs::slow_pitch);  pSlowDrift = P (ParameterIDs::slow_drift);
    pDriveOn = P (ParameterIDs::drive_on); pDriveMode = P (ParameterIDs::drive_mode);
    pDriveAmt = P (ParameterIDs::drive_amt); pDriveTone = P (ParameterIDs::drive_tone);
    pEnsOn = P (ParameterIDs::ens_on);     pEnsMode = P (ParameterIDs::ens_mode);      pEnsRate = P (ParameterIDs::ens_rate);
    pEnsDepth = P (ParameterIDs::ens_depth); pEnsMix = P (ParameterIDs::ens_mix);
    pWobOn = P (ParameterIDs::wob_on);     pWobWow = P (ParameterIDs::wob_wow);        pWobFlutter = P (ParameterIDs::wob_flutter);
    pWobAge = P (ParameterIDs::wob_age);   pWobDrop = P (ParameterIDs::wob_dropout);
    pHazeOn = P (ParameterIDs::haze_on);   pHazeSize = P (ParameterIDs::haze_size);    pHazeDecay = P (ParameterIDs::haze_decay);
    pHazePre = P (ParameterIDs::haze_predelay); pHazeMod = P (ParameterIDs::haze_mod); pHazeDamp = P (ParameterIDs::haze_damp);
    pHazeRev = P (ParameterIDs::haze_reverse);  pHazeFrz = P (ParameterIDs::haze_freeze); pHazeMix = P (ParameterIDs::haze_mix);
    pEchoOn = P (ParameterIDs::echo_on);   pEchoSync = P (ParameterIDs::echo_sync);    pEchoTime = P (ParameterIDs::echo_time);
    pEchoDiv = P (ParameterIDs::echo_div); pEchoFb = P (ParameterIDs::echo_fb);        pEchoTone = P (ParameterIDs::echo_tone);
    pEchoMix = P (ParameterIDs::echo_mix);

    pLfoShape = { P (ParameterIDs::lfo1_shape), P (ParameterIDs::lfo2_shape) };
    pLfoRate  = { P (ParameterIDs::lfo1_rate),  P (ParameterIDs::lfo2_rate) };
    pLfoSync  = { P (ParameterIDs::lfo1_sync),  P (ParameterIDs::lfo2_sync) };
    pLfoDest  = { P (ParameterIDs::lfo1_dest),  P (ParameterIDs::lfo2_dest) };
    pLfoAmt   = { P (ParameterIDs::lfo1_amt),   P (ParameterIDs::lfo2_amt) };
    pVapor    = P (ParameterIDs::vapor);

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
    wetBuf .setSize (2, samplesPerBlock, false, true, false);

    synth.prepare (sampleRate);
    synthLvlSm.reset (sampleRate, 0.02);
    synthLvlSm.setCurrentAndTargetValue (levelDbToGain (pSynthLvl->load()));
    confGain = 0.0f;
    confCoef = coef (25.0f);

    shiftDown.prepare (sampleRate, 40.0f);  shiftDown.setRatio (0.5f);
    shiftUp  .prepare (sampleRate, 40.0f);  shiftUp  .setRatio (2.0f);
    shiftFree.prepare (sampleRate, 40.0f);
    squareUp.reset();
    dryDelay.fill (0.0f);
    dryDelayPos = 0;

    for (auto* sm : { &octLvlSm, &octDownSm, &octUpSm, &freeMixSm })
        sm->reset (sampleRate, 0.02);
    octLvlSm .setCurrentAndTargetValue (levelDbToGain (pOctLvl->load()));
    octDownSm.setCurrentAndTargetValue (pOctDown->load());
    octUpSm  .setCurrentAndTargetValue (pOctUp->load());
    freeMixSm.setCurrentAndTargetValue (std::abs (pOctShift->load()) > 0.005f ? 1.0f : 0.0f);

    voiceEnv.prepare (sampleRate);
    envBuf.setSize (1, samplesPerBlock, false, true, false);
    ladder.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    ladder.reset();
    lastFltType = -1;

    fxSlow.prepare (sampleRate);  bpSlow.prepare (sampleRate);
    fxDrive.prepare (sampleRate); bpDrive.prepare (sampleRate);
    fxEns.prepare (sampleRate);   bpEns.prepare (sampleRate);
    fxWob.prepare (sampleRate);   bpWob.prepare (sampleRate);

    for (auto& l : lfos) l.prepare (sampleRate);
    modChunks.assign ((size_t) (samplesPerBlock / kFilterChunk + 2), {});
    volMod = 1.0f;
    volModCoef = coef (2.0f);

    fxHaze.prepare (sampleRate);  hazeActive = false;
    fxEcho.prepare (sampleRate);  echoActive = false;
    for (auto* s : { &hazeMix, &hazeOnS, &echoMix, &echoOnS })
    {
        s->prepare (sampleRate, 15.0f);
        s->snap (0.0f);
    }

    gateDetector = gateGain = envState = 0.0f;
    gateOpen = false;

    tracker.prepare (sampleRate);   // allocates its ring/diff buffers here, never in processBlock
    onset.prepare (sampleRate);
    lastTrackHpf = -1.0f;
    noteVelocity = 0.0f;
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
        wetBuf .setSize (2, numSamples, false, false, true);
        envBuf .setSize (1, numSamples, false, false, true);
    }
    const int numChunks = (numSamples + kFilterChunk - 1) / kFilterChunk;
    if ((int) modChunks.size() < numChunks)
        modChunks.resize ((size_t) numChunks);    // only if the host exceeds the prepared size

    // ---- host tempo / position (LFO sync, Echo sync) ------------------------------
    double ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) hostBpm = *bpm;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) ppq = *p;
        }

    // ---- modulation at control rate: LFO 1/2 → per-chunk destination offsets -------
    for (int k = 0; k < 2; ++k)
    {
        lfos[(size_t) k].setShape ((vs::Lfo::Shape) juce::jlimit (0, 4, (int) pLfoShape[(size_t) k]->load()));
        lfos[(size_t) k].beginBlock (pLfoRate[(size_t) k]->load(), pLfoSync[(size_t) k]->load() > 0.5f, hostBpm, ppq);
    }
    for (int c = 0; c < numChunks; ++c)
    {
        auto& mv = modChunks[(size_t) c];
        mv.clear();
        const int len = juce::jmin (kFilterChunk, numSamples - c * kFilterChunk);
        for (int k = 0; k < 2; ++k)
        {
            mv.add ((vs::ModDest) juce::jlimit (0, 5, (int) pLfoDest[(size_t) k]->load()),
                    lfos[(size_t) k].value(), pLfoAmt[(size_t) k]->load());
            lfos[(size_t) k].advance (len);
        }
    }

    // ---- VAPOR macro (offsets on top of the parameters; parameters are never written)
    const auto vapor = vs::VaporMacro::from (pVapor->load());

    const int numIn  = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int numOut = buffer.getNumChannels();

    // ---- read parameters (once per block) --------------------------------------
    inGainSm .setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    directSm .setTargetValue (levelDbToGain (pDirectLvl->load()) * vapor.directGain);
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    dryWetSm .setTargetValue (juce::jlimit (0.0f, 1.0f, pDryWet->load()));
    widthSm  .setTargetValue (juce::jlimit (0.0f, 2.0f, pWidth->load()));

    const float openThresh  = juce::Decibels::decibelsToGain (pGateThresh->load());
    const float closeThresh = openThresh * 0.7079f;                 // 3 dB hysteresis

    // ---- analysis settings ----------------------------------------------------------
    if (const float hpf = pTrackHpf->load(); hpf != lastTrackHpf)
    {
        tracker.setHighPassHz (hpf);
        lastTrackHpf = hpf;
    }
    const float sens = pTrackSens->load();
    tracker.setResponse (pTrackSpeed->load(), sens,
                         (vs::PitchTracker::Priority) juce::jlimit (0, 2, (int) pNotePrio->load()));
    onset.setSensitivity (sens, pVelSens->load());

    // ---- synth settings ---------------------------------------------------------------
    {
        vs::SynthSource::Params sp;
        sp.wave1 = (vs::SynthSource::Wave) juce::jlimit (0, 4, (int) pOsc1Wave->load());
        sp.wave2 = (vs::SynthSource::Wave) juce::jlimit (0, 4, (int) pOsc2Wave->load());
        sp.oct1 = juce::roundToInt (pOsc1Oct->load());
        sp.oct2 = juce::roundToInt (pOsc2Oct->load());
        sp.detune2Cents = pOsc2Det->load();
        sp.oscMix = pOscMix->load();
        sp.pulseWidth = pPulseW->load();
        sp.unison = juce::roundToInt (pUnison->load());
        sp.spread = pSpread->load();
        sp.subLevel = pSubLvl->load();
        sp.glideMs = pGlide->load();
        synth.setParams (sp);
    }
    synthLvlSm.setTargetValue (levelDbToGain (pSynthLvl->load()) * vapor.synthGain);

    // ---- octave layer + square-up settings ------------------------------------------------
    squareUp.setAmount (pDirectSq->load());
    octLvlSm .setTargetValue (levelDbToGain (pOctLvl->load()));
    octDownSm.setTargetValue (pOctDown->load());
    octUpSm  .setTargetValue (pOctUp->load());
    const float freeSt = pOctShift->load();
    const bool freeOn = std::abs (freeSt) > 0.005f;     // free shifter is skipped at 0 st
    shiftFree.setRatio (std::exp2 (freeSt / 12.0f));
    freeMixSm.setTargetValue (freeOn ? 1.0f : 0.0f);
    const bool freeRun = freeOn || freeMixSm.isSmoothing();
    // ---- voice settings ----------------------------------------------------------------
    voiceEnv.setParams ((vs::VoiceEnvelope::Mode) juce::jlimit (0, 2, (int) pEnvMode->load()),
                        juce::jmin (5000.0f, pAtk->load() * vapor.attackMul), pDec->load(), pSus->load(), pRel->load());
    voiceEnv.setSwellBlend (vapor.swellBlend);
    if (const int ft = juce::jlimit (0, 3, (int) pFltType->load()); ft != lastFltType)
    {
        using M = juce::dsp::LadderFilterMode;
        static constexpr M modes[] = { M::LPF24, M::LPF12, M::BPF12, M::HPF12 };
        ladder.setMode (modes[ft]);
        lastFltType = ft;
    }
    ladder.setResonance (0.95f * juce::jlimit (0.0f, 1.0f, pRes->load()));
    ladder.setDrive (1.0f + 9.0f * juce::jlimit (0.0f, 1.0f, pFltDrv->load()));
    const float cutoffOct = std::log2 (juce::jlimit (20.0f, 20000.0f, pCutoff->load()));
    const float envAmt = juce::jlimit (-1.0f, 1.0f, pEnvAmt->load());
    auto* envLvl = envBuf.getWritePointer (0);

    constexpr int dryLatency = vs::SquareUp::getLatency();
    static_assert (dryLatency < 64, "dryDelay too short for the oversampler latency");

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
    auto* wetL = wetBuf.getWritePointer (0);
    auto* wetR = wetBuf.getWritePointer (1);

    float pitchRatio = 1.0f, volTarget = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        if ((i % kFilterChunk) == 0)                  // control-rate modulation tick
        {
            const auto& mv = modChunks[(size_t) (i / kFilterChunk)];
            pitchRatio = std::exp2 (mv.pitchSt / 12.0f);
            synth.setPulseWidthOffset (mv.pwm);
            volTarget = mv.volume;
        }
        volMod = volTarget + volModCoef * (volMod - volTarget);

        const float x = mono[i] * inGainSm.getNextValue();

        // dry path, delayed to line up with the oversampled square-up on source A
        dryDelay[(size_t) dryDelayPos] = x;
        dry[i] = dryDelay[(size_t) ((dryDelayPos - dryLatency + 64) & 63)];
        dryDelayPos = (dryDelayPos + 1) & 63;
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

        // analysis path (control signals only; never touches the audio)
        tracker.push (x, gateOpen);
        // envelope follower
        const float ag = std::abs (gated);
        envState = ag > envState ? envAtkCoef * (envState - ag) + ag
                                 : envRelCoef * (envState - ag) + ag;

        // pick onset → voice envelope (Follow / ADSR / Swell)
        if (onset.push (x, openThresh))
        {
            noteVelocity = onset.getVelocity();
            voiceEnv.noteOn (noteVelocity, envState);
        }
        voiceEnv.tick (envState, gateOpen);
        const float envLevel = voiceEnv.level();
        const float contour  = voiceEnv.shape();     // A/B gain: 1 in Follow, contour otherwise
        envLvl[i] = envLevel;

        // [A] Direct source (mono, centred), optionally squared-up (2× oversampled)
        const float direct = squareUp.process (gated) * directSm.getNextValue();

        // [B] Octave layer: causal rotating-head shifters (chord-safe)
        const float down = shiftDown.process (gated);
        const float up   = shiftUp.process (gated);
        const float fm   = freeMixSm.getNextValue();
        const float free = freeRun ? shiftFree.process (gated) * fm : 0.0f;
        const float octave = (octDownSm.getNextValue() * down + octUpSm.getNextValue() * up + free)
                           * octLvlSm.getNextValue();

        // [C] Synth source: fully gated by the voice envelope, and faded out when
        // tracking confidence is low (chords) so wrong notes never sound.
        // Confidence only fades while the gate is open; ADSR release tails keep playing.
        const float confTarget = gateOpen ? juce::jlimit (0.0f, 1.0f, (tracker.getConfidence() - 0.35f) / 0.3f)
                                          : confGain;
        confGain = confTarget + confCoef * (confGain - confTarget);
        const float synthAmp = envLevel * confGain * synthLvlSm.getNextValue();
        float sl = 0.0f, sr = 0.0f;
        synth.tick (tracker.getHz() * pitchRatio, synthAmp, sl, sr);

        const float ab = (direct + octave) * contour;
        wetL[i] = (ab + sl) * volMod;
        wetR[i] = (ab + sr) * volMod;
    }

    // ---- Synth Voice filter: ladder on the mixed sources, cutoff modulated every
    //      kFilterChunk samples by the voice envelope (±6 octaves at full amount) ----
    for (int start = 0; start < numSamples; start += kFilterChunk)
    {
        const int len = juce::jmin (kFilterChunk, numSamples - start);
        float e = 0.0f;
        for (int k = 0; k < len; ++k) e += envLvl[start + k];
        e /= (float) len;
        const float hz = std::exp2 (cutoffOct + envAmt * e * 6.0f + modChunks[(size_t) (start / kFilterChunk)].cutoffOct);
        ladder.setCutoffFrequencyHz (juce::jlimit (20.0f, (float) (0.45 * getSampleRate()), hz));
        juce::dsp::AudioBlock<float> blk (wetBuf);
        auto sub = blk.getSubBlock ((size_t) start, (size_t) len);
        ladder.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }

    // ---- FX chain I (fixed order, each with a 10 ms bypass crossfade) -------------
    // block-rate destinations take the LFO value at the start of the block
    const auto& mod0 = modChunks[0];
    const bool slowOn = pSlowOn->load() > 0.5f;

    fxSlow.setParams (pSlowPitch->load() + (slowOn ? vapor.slowPitchSt : 0.0f), pSlowDrift->load());
    fxDrive.setParams ((vs::Drive::Mode) juce::jlimit (0, 3, (int) pDriveMode->load()), pDriveAmt->load(), pDriveTone->load());
    fxEns.setParams ((vs::Ensemble::Mode) juce::jlimit (0, 2, (int) pEnsMode->load()), pEnsRate->load(),
                     pEnsDepth->load() + vapor.ensDepth + mod0.ensDepth, pEnsMix->load());
    fxWob.setParams (pWobWow->load() + vapor.wobWow, pWobFlutter->load(), pWobAge->load() + vapor.wobAge, pWobDrop->load());

    vs::runInsert (bpSlow,  slowOn,                                         fxSlow,  wetL, wetR, numSamples);
    vs::runInsert (bpDrive, pDriveOn->load() > 0.5f,                        fxDrive, wetL, wetR, numSamples);
    vs::runInsert (bpEns,   pEnsOn->load()   > 0.5f,                        fxEns,   wetL, wetR, numSamples);
    vs::runInsert (bpWob,   pWobOn->load()   > 0.5f || vapor.engageWobble,  fxWob,   wetL, wetR, numSamples);

    // ---- FX chain II: Haze → Echo (tails spill over when switched off) -------------
    {
        vs::Haze::Params hp;
        hp.size = pHazeSize->load() + mod0.hazeSize;  hp.decayS = pHazeDecay->load();  hp.predelayMs = pHazePre->load();
        hp.mod = pHazeMod->load();        hp.damp = pHazeDamp->load();     hp.reverse = pHazeRev->load();
        hp.freeze = pHazeFrz->load() > 0.5f;
        hp.on = pHazeOn->load() > 0.5f || vapor.engageHaze;
        fxHaze.setParams (hp);
        vs::runSend (fxHaze, hp.on, juce::jmin (1.0f, pHazeMix->load() + vapor.hazeMix),
                     hazeMix, hazeOnS, hazeActive, wetL, wetR, numSamples);
    }
    {
        vs::Echo::Params ep;
        ep.sync = pEchoSync->load() > 0.5f;  ep.timeMs = pEchoTime->load();  ep.division = (int) pEchoDiv->load();
        ep.bpm = hostBpm;                    ep.feedback = pEchoFb->load();  ep.tone = pEchoTone->load();
        ep.on = pEchoOn->load() > 0.5f;
        fxEcho.setParams (ep);
        vs::runSend (fxEcho, ep.on, pEchoMix->load(), echoMix, echoOnS, echoActive, wetL, wetR, numSamples);
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
        const float w = widthSm.getNextValue();

        // M/S width on the wet bus (0 = mono, 1 = as is, 2 = double side)
        const float mid  = 0.5f * (wetL[i] + wetR[i]);
        const float side = 0.5f * (wetL[i] - wetR[i]) * w;

        const float yl = softClip ((dry[i] * gDry + (mid + side) * gWet) * g);
        const float yr = softClip ((dry[i] * gDry + (mid - side) * gWet) * g);
        outPk = juce::jmax (outPk, std::abs (yl), std::abs (yr));
        if (outR != nullptr) { outL[i] = yl; outR[i] = yr; }
        else                 { outL[i] = 0.5f * (yl + yr); }
    }

    for (int ch = 2; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    VaporMeters::raise (meters.inPeak, inPk);
    VaporMeters::raise (meters.outPeak, outPk);
    meters.env.store (voiceEnv.level(), std::memory_order_relaxed);
    meters.gate.store (gateOpen, std::memory_order_relaxed);
    meters.noteHz.store (gateOpen ? tracker.getHz() : 0.0f, std::memory_order_relaxed);
    meters.conf.store (tracker.getConfidence(), std::memory_order_relaxed);
    meters.velocity.store (noteVelocity, std::memory_order_relaxed);
    meters.onsets.store (onset.getOnsetCount(), std::memory_order_relaxed);
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
