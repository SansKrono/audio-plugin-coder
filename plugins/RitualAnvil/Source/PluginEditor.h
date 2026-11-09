#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class RitualAnvilAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit RitualAnvilAudioProcessorEditor (RitualAnvilAudioProcessor&);
    ~RitualAnvilAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    RitualAnvilAudioProcessor& audioProcessor;

    // ═══════════════════════════════════════════════════════════════════
    // CRITICAL: Member declaration order (prevents DAW crashes)
    // Members are destroyed in REVERSE order: Attachments → WebView → Relays
    // See: .claude/troubleshooting/resolutions/webview-member-order-crash.md
    // ═══════════════════════════════════════════════════════════════════

    // 1. RELAYS FIRST (destroyed last)
    juce::WebSliderRelay mixReadyRelay         { ParameterIDs::mixReady };
    juce::WebSliderRelay inputGainRelay        { ParameterIDs::inputGain };
    juce::WebSliderRelay hpfRelay              { ParameterIDs::hpf };
    juce::WebSliderRelay transientAttackRelay  { ParameterIDs::transientAttack };
    juce::WebSliderRelay transientSustainRelay { ParameterIDs::transientSustain };
    juce::WebSliderRelay eqKickRelay           { ParameterIDs::eqKick };
    juce::WebSliderRelay eqBoxRelay            { ParameterIDs::eqBox };
    juce::WebSliderRelay eqSnareRelay          { ParameterIDs::eqSnare };
    juce::WebSliderRelay eqPresenceRelay       { ParameterIDs::eqPresence };
    juce::WebSliderRelay eqAirRelay            { ParameterIDs::eqAir };
    juce::WebSliderRelay punchRelay            { ParameterIDs::punch };
    juce::WebSliderRelay heatRelay             { ParameterIDs::heat };
    juce::WebSliderRelay widthRelay            { ParameterIDs::width };
    juce::WebSliderRelay ceilingRelay          { ParameterIDs::ceiling };
    juce::WebToggleButtonRelay bypassRelay     { ParameterIDs::bypass };

    /** Slider relays in anvil::P order (matches anvil::sliderSpecs). */
    std::array<juce::WebSliderRelay*, anvil::NumSliderParams> sliderRelays {
        &mixReadyRelay, &inputGainRelay, &hpfRelay, &transientAttackRelay, &transientSustainRelay,
        &eqKickRelay, &eqBoxRelay, &eqSnareRelay, &eqPresenceRelay, &eqAirRelay,
        &punchRelay, &heatRelay, &widthRelay, &ceilingRelay
    };

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (destroyed first)
    std::array<std::unique_ptr<juce::WebSliderParameterAttachment>, anvil::NumSliderParams> sliderAttachments;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    // Resource provider
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::ZipFile* getZipFile();
    static const char* getMimeForExtension (const juce::String& extension);
    static std::vector<std::byte> streamToVector (juce::InputStream& stream);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualAnvilAudioProcessorEditor)
};
