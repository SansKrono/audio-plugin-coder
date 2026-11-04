#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
VaporStringsAudioProcessorEditor::VaporStringsAudioProcessorEditor (VaporStringsAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    using namespace ParamTable;

    // ---- 1. relays (one per parameter, named by parameter ID) ----------------
    for (const auto& d : defs)
    {
        switch (d.type)
        {
            case Type::Bool:   toggleRelays.emplace_back (d.id); break;
            case Type::Choice: comboRelays .emplace_back (d.id); break;
            case Type::Int:
            case Type::Float:  sliderRelays.emplace_back (d.id); break;
        }
    }

    // ---- 2. WebBrowserComponent (WebView2, native integration, resource provider)
    auto options = juce::WebBrowserComponent::Options()
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                     .withUserDataFolder (juce::File::getSpecialLocation (
                                         juce::File::SpecialLocationType::tempDirectory))
                                     .withBackgroundColour (juce::Colour (0xff0d0221))) // --surface: no drag-flash
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const auto& url) { return getResource (url); })
        .withNativeFunction ("setUiScale",
            [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() > 0)
                    setUiScaleAndSize ((double) args[0]);
                complete ({});
            })
        .withNativeFunction ("resizeGripStart",
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                gripStartW = getWidth();
                gripStartH = getHeight();
                complete ({});
            })
        .withNativeFunction ("resizeGrip",
            [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() >= 1 && gripStartW > 0)
                {
                    const int lo = juce::roundToInt (kPlateW * kMinUiScale);
                    const int hi = juce::roundToInt (kPlateW * kMaxUiScale);
                    const int newW = juce::jlimit (lo, hi, gripStartW + (int) args[0]);
                    setSize (newW, juce::roundToInt (newW / kAspect));
                }
                complete ({});
            })
        .withNativeFunction ("resizeGripEnd",
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                gripStartW = gripStartH = 0;
                audioProcessor.apvts.state.setProperty ("uiScale", (double) getWidth() / kPlateW, nullptr);
                complete ({});
            })
        .withNativeFunction ("getUiPrefs",
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                auto* prefs = new juce::DynamicObject();
                prefs->setProperty ("uiScale", (double) audioProcessor.apvts.state.getProperty ("uiScale", 1.0));
                complete (juce::var (prefs));
            })
        .withNativeFunction ("presetCurrent",
            [this] (const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                complete (presetInfo());
            })
        .withNativeFunction ("presetSelect",
            [this] (const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion complete)
            {
                if (args.size() > 0)
                    audioProcessor.setCurrentProgram ((int) args[0]);
                complete (presetInfo());
            });

    for (auto& r : sliderRelays) options = options.withOptionsFrom (r);
    for (auto& r : toggleRelays) options = options.withOptionsFrom (r);
    for (auto& r : comboRelays)  options = options.withOptionsFrom (r);

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);

    // ---- 3. attachments AFTER the WebBrowserComponent --------------------------
    {
        auto sIt = sliderRelays.begin();
        auto tIt = toggleRelays.begin();
        auto cIt = comboRelays.begin();

        for (const auto& d : defs)
        {
            auto& param = *audioProcessor.apvts.getParameter (d.id);
            switch (d.type)
            {
                case Type::Bool:
                    toggleAttachments.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (param, *tIt++, nullptr));
                    break;
                case Type::Choice:
                    comboAttachments.push_back (std::make_unique<juce::WebComboBoxParameterAttachment> (param, *cIt++, nullptr));
                    break;
                case Type::Int:
                case Type::Float:
                    sliderAttachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (param, *sIt++, nullptr));
                    break;
            }
        }
    }

    // ---- window: scalable, aspect locked, in-page grip ----------------------------
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kPlateW * kMinUiScale), juce::roundToInt (kPlateH * kMinUiScale),
                     juce::roundToInt (kPlateW * kMaxUiScale), juce::roundToInt (kPlateH * kMaxUiScale));
    if (auto* bc = getConstrainer())
        bc->setFixedAspectRatio (kAspect);

    const double uiScale = juce::jlimit (kMinUiScale, kMaxUiScale,
        (double) audioProcessor.apvts.state.getProperty ("uiScale", 1.0));
    setSize (juce::roundToInt (kPlateW * uiScale), juce::roundToInt (kPlateH * uiScale));

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    startTimerHz (30);
}

VaporStringsAudioProcessorEditor::~VaporStringsAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
juce::var VaporStringsAudioProcessorEditor::presetInfo() const
{
    auto* o = new juce::DynamicObject();
    const int idx = const_cast<VaporStringsAudioProcessor&> (audioProcessor).getCurrentProgram();
    o->setProperty ("index", idx);
    o->setProperty ("name", const_cast<VaporStringsAudioProcessor&> (audioProcessor).getProgramName (idx));
    return juce::var (o);
}

void VaporStringsAudioProcessorEditor::timerCallback()
{
    auto& m = audioProcessor.meters;

    auto* o = new juce::DynamicObject();
    o->setProperty ("in",   (double) m.inPeak.exchange (0.0f));
    o->setProperty ("out",  (double) m.outPeak.exchange (0.0f));
    o->setProperty ("env",  (double) m.env.load());
    o->setProperty ("conf", (double) m.conf.load());
    o->setProperty ("gate", m.gate.load() ? 1 : 0);

    const float hz = m.noteHz.load();
    if (hz > 20.0f)
    {
        const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
        const int note = juce::roundToInt (midi);
        o->setProperty ("note", note);
        o->setProperty ("cents", (midi - note) * 100.0);
    }
    else
    {
        o->setProperty ("note", -1);
        o->setProperty ("cents", 0.0);
    }

    webView->emitEventIfBrowserIsVisible ("meters", juce::var (o));
}

//==============================================================================
void VaporStringsAudioProcessorEditor::setUiScaleAndSize (double s)
{
    s = juce::jlimit (kMinUiScale, kMaxUiScale, s);
    audioProcessor.apvts.state.setProperty ("uiScale", s, nullptr);
    setSize (juce::roundToInt (kPlateW * s), juce::roundToInt (kPlateH * s));
}

void VaporStringsAudioProcessorEditor::snapWindowToAspect()
{
    // Host-initiated resizes bypass the constrainer; re-assert the plate aspect.
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const double current = (double) getWidth() / (double) getHeight();
    if (std::abs (current - kAspect) < 0.001)
        return;

    setSize (getWidth(), juce::roundToInt (getWidth() / kAspect));
}

void VaporStringsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0221));
}

void VaporStringsAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
    snapWindowToAspect();
}

//==============================================================================
// Resource provider: serves the single inlined index.html from BinaryData.
std::optional<juce::WebBrowserComponent::Resource> VaporStringsAudioProcessorEditor::getResource (const juce::String& url)
{
    const auto path = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false)
                         .upToFirstOccurrenceOf ("?", false, false)
                         .trimCharactersAtStart ("/");

    if (path.isEmpty() || path == "index.html")
    {
        int size = 0;
        if (const auto* data = BinaryData::getNamedResource ("index_html", size))
        {
            std::vector<std::byte> bytes ((size_t) size);
            std::memcpy (bytes.data(), data, (size_t) size);
            return juce::WebBrowserComponent::Resource { std::move (bytes), "text/html" };
        }
    }

    return std::nullopt;
}
