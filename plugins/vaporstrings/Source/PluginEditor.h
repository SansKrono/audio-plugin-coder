#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <deque>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

//==============================================================================
class VaporStringsAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit VaporStringsAudioProcessorEditor (VaporStringsAudioProcessor&);
    ~VaporStringsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    VaporStringsAudioProcessor& audioProcessor;

    // UI layout contract (Design/v1-ui-map.json: 65 x 40 cells @ 16 px, scalable 0.5-2.0)
    static constexpr int    kPlateW     = 1040;
    static constexpr int    kPlateH     = 640;
    static constexpr double kAspect     = (double) kPlateW / (double) kPlateH;
    static constexpr double kMinUiScale = 0.5;
    static constexpr double kMaxUiScale = 2.0;

    void snapWindowToAspect();
    void setUiScaleAndSize (double s);
    juce::var presetInfo() const;

    int gripStartW = 0;
    int gripStartH = 0;

    // ═══════════════════════════════════════════════════════════════════
    // CRITICAL: Member Declaration Order (prevents DAW crashes)
    // C++ destroys members in REVERSE order: Relays → WebView → Attachments
    // See: .agents/troubleshooting/resolutions/webview-member-order-crash.md
    // ═══════════════════════════════════════════════════════════════════

    // 1. PARAMETER RELAYS FIRST (destroyed last). std::deque keeps element
    //    addresses stable while relays are appended in the constructor.
    std::deque<juce::WebSliderRelay> sliderRelays;
    std::deque<juce::WebToggleButtonRelay> toggleRelays;
    std::deque<juce::WebComboBoxRelay> comboRelays;

    // 2. WEBVIEW SECOND (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS LAST (destroyed first)
    using SliderAttachment = std::unique_ptr<juce::WebSliderParameterAttachment>;
    using ToggleAttachment = std::unique_ptr<juce::WebToggleButtonParameterAttachment>;
    using ComboAttachment  = std::unique_ptr<juce::WebComboBoxParameterAttachment>;
    std::vector<SliderAttachment> sliderAttachments;
    std::vector<ToggleAttachment> toggleAttachments;
    std::vector<ComboAttachment> comboAttachments;

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaporStringsAudioProcessorEditor)
};
