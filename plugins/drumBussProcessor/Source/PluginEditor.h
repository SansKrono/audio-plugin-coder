/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

//==============================================================================
/**
*/
class drumBussProcessorAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    drumBussProcessorAudioProcessorEditor (drumBussProcessorAudioProcessor&);
    ~drumBussProcessorAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    drumBussProcessorAudioProcessor& audioProcessor;

    // ═══════════════════════════════════════════════════════════════════
    // CRITICAL: Member Declaration Order (Prevents DAW Crashes)
    // ═══════════════════════════════════════════════════════════════════
    //
    // C++ destroys members in REVERSE order of declaration.
    // WebView references relays via .withOptionsFrom()
    //
    // CORRECT ORDER: Relays → WebView → Attachments
    //
    // This ensures WebView is destroyed BEFORE the relays it references,
    // preventing access to freed memory (segfault/crash).
    //
    // See: .claude/troubleshooting/resolutions/webview-member-order-crash.md
    // ═══════════════════════════════════════════════════════════════════

    // 1. PARAMETER RELAYS FIRST (no dependencies, destroyed last)
    // Relays are direct member variables - initialized with parameter ID
    juce::WebSliderRelay inputTrimRelay { "input_trim" };
    juce::WebSliderRelay mixReadyRelay { "mix_ready" };
    juce::WebSliderRelay outputTrimRelay { "output_trim" };
    juce::WebToggleButtonRelay bypassRelay { "bypass" };

    // 2. WEBVIEW SECOND (depends on relays, destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS LAST (depend on relays + parameters, destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> inputTrimAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> mixReadyAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputTrimAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    // Resource provider function
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    // Helper functions for resource provider
    static juce::ZipFile* getZipFile();
    static const char* getMimeForExtension (const juce::String& extension);
    static juce::String getExtension (juce::String filename);
    static std::vector<std::byte> streamToVector (juce::InputStream& stream);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (drumBussProcessorAudioProcessorEditor)
};
