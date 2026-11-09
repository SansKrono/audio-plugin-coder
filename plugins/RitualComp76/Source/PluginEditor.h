#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class RitualComp76AudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit RitualComp76AudioProcessorEditor (RitualComp76AudioProcessor&);
    ~RitualComp76AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    RitualComp76AudioProcessor& audioProcessor;

    // ═══════════════════════════════════════════════════════════════════
    // CRITICAL: Member declaration order (prevents DAW crashes)
    // Members are destroyed in REVERSE order: Relays → WebView → Attachments
    // See: .claude/troubleshooting/resolutions/webview-member-order-crash.md
    // ═══════════════════════════════════════════════════════════════════

    // 1. RELAYS FIRST (destroyed last)
    juce::WebSliderRelay        inputRelay     { ParameterIDs::input };
    juce::WebSliderRelay        outputRelay    { ParameterIDs::output };
    juce::WebSliderRelay        attackRelay    { ParameterIDs::attack };
    juce::WebSliderRelay        releaseRelay   { ParameterIDs::release };
    juce::WebComboBoxRelay      ratioRelay     { ParameterIDs::ratio };
    juce::WebComboBoxRelay      meterModeRelay { ParameterIDs::meterMode };
    juce::WebToggleButtonRelay  bypassRelay    { ParameterIDs::bypass };

    // 2. WEBVIEW SECOND
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment>       inputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       outputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       attackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       releaseAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     ratioAttachment;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     meterModeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    // Resource provider
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    static juce::ZipFile* getZipFile();
    static const char* getMimeForExtension (const juce::String& extension);
    static std::vector<std::byte> streamToVector (juce::InputStream& stream);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualComp76AudioProcessorEditor)
};
