#include "PluginEditor.h"
#include "RitualAnvilWebUI.h"

#include <unordered_map>

namespace
{
    constexpr int editorWidth  = 1000;
    constexpr int editorHeight = 600;
    constexpr int meterHz      = 30;
}

//==============================================================================
RitualAnvilAudioProcessorEditor::RitualAnvilAudioProcessorEditor (RitualAnvilAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    auto options = juce::WebBrowserComponent::Options{};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                  .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)));
   #endif

    // Relays (members) → WebBrowserComponent
    options = options.withNativeIntegrationEnabled()
                     .withResourceProvider ([this] (const auto& url) { return getResource (url); });
    for (auto* relay : sliderRelays)
        options = options.withOptionsFrom (*relay);
    options = options.withOptionsFrom (bypassRelay);

    webView = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webView);

    // Attachments AFTER the WebBrowserComponent
    auto& apvts = audioProcessor.apvts;
    for (int i = 0; i < anvil::NumSliderParams; ++i)
        sliderAttachments[(size_t) i] = std::make_unique<juce::WebSliderParameterAttachment> (
            *apvts.getParameter (anvil::sliderSpecs[(size_t) i].id), *sliderRelays[(size_t) i], nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter (ParameterIDs::bypass), bypassRelay, nullptr);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Size LAST: resized() must see the WebBrowserComponent, or it stays 0x0.
    setSize (editorWidth, editorHeight);

    startTimerHz (meterHz);
}

RitualAnvilAudioProcessorEditor::~RitualAnvilAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void RitualAnvilAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1f));
}

void RitualAnvilAudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

/** Native → JS "meter" event: { in, out, punchGr, limGr, eff: { <paramId>: value } } */
void RitualAnvilAudioProcessorEditor::timerCallback()
{
    const auto m = audioProcessor.popMeters();

    auto* eff = new juce::DynamicObject();
    for (int i = 0; i < anvil::NumSliderParams; ++i)
        eff->setProperty (anvil::sliderSpecs[(size_t) i].id, audioProcessor.getEffectiveValue (i));

    auto* payload = new juce::DynamicObject();
    payload->setProperty ("in", m.inDbfs);
    payload->setProperty ("out", m.outDbfs);
    payload->setProperty ("punchGr", m.punchGrDb);
    payload->setProperty ("limGr", m.limiterGrDb);
    payload->setProperty ("eff", juce::var (eff));
    webView->emitEventIfBrowserIsVisible ("meter", juce::var (payload));
}

//==============================================================================
// Resource provider: serves Source/ui/public from the embedded zip
//==============================================================================
juce::ZipFile* RitualAnvilAudioProcessorEditor::getZipFile()
{
    static auto stream = std::make_unique<juce::MemoryInputStream> (RitualAnvilWebUI::RitualAnvil_WebUI_zip,
                                                                    (size_t) RitualAnvilWebUI::RitualAnvil_WebUI_zipSize,
                                                                    false);
    static auto zip = std::make_unique<juce::ZipFile> (stream.get(), false);
    return zip.get();
}

const char* RitualAnvilAudioProcessorEditor::getMimeForExtension (const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { "htm",  "text/html" },        { "html", "text/html" },
        { "txt",  "text/plain" },       { "css",  "text/css" },
        { "js",   "text/javascript" },  { "mjs",  "text/javascript" },
        { "json", "application/json" }, { "map",  "application/json" },
        { "svg",  "image/svg+xml" },    { "png",  "image/png" },
        { "jpg",  "image/jpeg" },       { "jpeg", "image/jpeg" },
        { "ico",  "image/vnd.microsoft.icon" },
        { "woff2", "font/woff2" }
    };

    if (const auto it = mimeMap.find (extension.toLowerCase()); it != mimeMap.end())
        return it->second;

    jassertfalse;
    return "text/plain";
}

std::vector<std::byte> RitualAnvilAudioProcessorEditor::streamToVector (juce::InputStream& stream)
{
    std::vector<std::byte> result ((size_t) stream.getTotalLength());
    stream.setPosition (0);
    [[maybe_unused]] const auto bytesRead = stream.read (result.data(), result.size());
    jassert (bytesRead == (juce::ssize_t) result.size());
    return result;
}

std::optional<juce::WebBrowserComponent::Resource> RitualAnvilAudioProcessorEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html")
                                 : url.fromFirstOccurrenceOf ("/", false, false).upToFirstOccurrenceOf ("?", false, false);

    if (auto* archive = getZipFile())
    {
        // cmake -E tar stores entries as "./path"; accept both spellings
        auto* entry = archive->getEntry (path);
        if (entry == nullptr)
            entry = archive->getEntry ("./" + path);

        if (entry != nullptr)
        {
            auto stream = juce::rawToUniquePtr (archive->createStreamForEntry (*entry));
            if (stream != nullptr)
                return juce::WebBrowserComponent::Resource { streamToVector (*stream),
                                                             juce::String (getMimeForExtension (path.fromLastOccurrenceOf (".", false, false))) };
        }
    }

    return std::nullopt;
}
