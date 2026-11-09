#include "PluginEditor.h"
#include "RitualComp76WebUI.h"

#include <unordered_map>

namespace
{
    constexpr int editorWidth  = 760;
    constexpr int editorHeight = 380;
    constexpr int meterHz      = 30;
    constexpr float dbfsToDbu  = 18.0f;   // 0 dBFS == +18 dBu (VU +4/+8 reference math lives in the UI)
}

//==============================================================================
RitualComp76AudioProcessorEditor::RitualComp76AudioProcessorEditor (RitualComp76AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    auto options = juce::WebBrowserComponent::Options{};

   #if JUCE_WINDOWS
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                  .withUserDataFolder (juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory)));
   #endif

    // Relays (members) → WebBrowserComponent
    webView = std::make_unique<juce::WebBrowserComponent> (
        options.withNativeIntegrationEnabled()
               .withOptionsFrom (inputRelay)
               .withOptionsFrom (outputRelay)
               .withOptionsFrom (attackRelay)
               .withOptionsFrom (releaseRelay)
               .withOptionsFrom (ratioRelay)
               .withOptionsFrom (meterModeRelay)
               .withOptionsFrom (bypassRelay)
               .withResourceProvider ([this] (const auto& url) { return getResource (url); }));

    addAndMakeVisible (*webView);

    // Attachments AFTER the WebBrowserComponent
    auto& apvts = audioProcessor.apvts;
    inputAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter (ParameterIDs::input), inputRelay, nullptr);
    outputAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter (ParameterIDs::output), outputRelay, nullptr);
    attackAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter (ParameterIDs::attack), attackRelay, nullptr);
    releaseAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter (ParameterIDs::release), releaseRelay, nullptr);
    ratioAttachment     = std::make_unique<juce::WebComboBoxParameterAttachment> (*apvts.getParameter (ParameterIDs::ratio), ratioRelay, nullptr);
    meterModeAttachment = std::make_unique<juce::WebComboBoxParameterAttachment> (*apvts.getParameter (ParameterIDs::meterMode), meterModeRelay, nullptr);
    bypassAttachment    = std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter (ParameterIDs::bypass), bypassRelay, nullptr);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Size LAST: resized() must see the WebBrowserComponent, or it stays 0x0.
    setSize (editorWidth, editorHeight);

    startTimerHz (meterHz);
}

RitualComp76AudioProcessorEditor::~RitualComp76AudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void RitualComp76AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1f));
}

void RitualComp76AudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void RitualComp76AudioProcessorEditor::timerCallback()
{
    auto* payload = new juce::DynamicObject();
    payload->setProperty ("gr",  audioProcessor.popGainReductionDb());
    payload->setProperty ("out", audioProcessor.popOutputLevelDbfs() + dbfsToDbu);
    webView->emitEventIfBrowserIsVisible ("meter", juce::var (payload));
}

//==============================================================================
// Resource provider: serves Source/ui/public from the embedded zip
//==============================================================================
juce::ZipFile* RitualComp76AudioProcessorEditor::getZipFile()
{
    static auto stream = std::make_unique<juce::MemoryInputStream> (RitualComp76WebUI::RitualComp76_WebUI_zip,
                                                                    (size_t) RitualComp76WebUI::RitualComp76_WebUI_zipSize,
                                                                    false);
    static auto zip = std::make_unique<juce::ZipFile> (stream.get(), false);
    return zip.get();
}

const char* RitualComp76AudioProcessorEditor::getMimeForExtension (const juce::String& extension)
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

std::vector<std::byte> RitualComp76AudioProcessorEditor::streamToVector (juce::InputStream& stream)
{
    std::vector<std::byte> result ((size_t) stream.getTotalLength());
    stream.setPosition (0);
    [[maybe_unused]] const auto bytesRead = stream.read (result.data(), result.size());
    jassert (bytesRead == (juce::ssize_t) result.size());
    return result;
}

std::optional<juce::WebBrowserComponent::Resource> RitualComp76AudioProcessorEditor::getResource (const juce::String& url)
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
