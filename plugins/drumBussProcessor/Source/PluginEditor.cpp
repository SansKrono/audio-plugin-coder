/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.hpp"

#include "BinaryData.h"

// namespace BinaryData
// {
//     extern const char* drumbussprocessor_WebUI_zip;
//     extern const int drumbussprocessor_WebUI_zipSize;
// }

//==============================================================================
drumBussProcessorAudioProcessorEditor::drumBussProcessorAudioProcessorEditor (drumBussProcessorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set window size (adjust to match your UI design)
    setSize (940, 600);

    // CRITICAL: Parameter relays are member variables, already initialized in header
    // They are passed to WebBrowserComponent via .withOptionsFrom()

    // CRITICAL: Create WebBrowserComponent with proper Windows WebView2 configuration
    juce::WebBrowserComponent::Options options;
    
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2);
    
    #if JUCE_WINDOWS
    options = options.withWinWebView2Options(
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder(juce::File::getSpecialLocation(
                        juce::File::SpecialLocationType::tempDirectory)));
    #endif

    webView = std::make_unique<juce::WebBrowserComponent>(
        options
            .withNativeIntegrationEnabled()
            .withOptionsFrom(inputTrimRelay)
            .withOptionsFrom(mixReadyRelay)
            .withOptionsFrom(outputTrimRelay)
            .withOptionsFrom(bypassRelay)
            .withResourceProvider([this](const auto& url) {
                return getResource(url);
            })
    );

    addAndMakeVisible(*webView);

    // CRITICAL: Create parameter attachments AFTER WebBrowserComponent
    inputTrimAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("input_trim"), inputTrimRelay, nullptr);
    mixReadyAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("mix_ready"), mixReadyRelay, nullptr);
    outputTrimAttachment = std::make_unique<juce::WebSliderParameterAttachment>(*audioProcessor.apvts.getParameter("output_trim"), outputTrimRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(*audioProcessor.apvts.getParameter("bypass"), bypassRelay, nullptr);

    // CRITICAL: Load embedded web content (NOT data URI!)
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
}

drumBussProcessorAudioProcessorEditor::~drumBussProcessorAudioProcessorEditor()
{
}

//==============================================================================
void drumBussProcessorAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void drumBussProcessorAudioProcessorEditor::resized()
{
    // Make WebView fill the entire editor window
    webView->setBounds(getLocalBounds());
}

//==============================================================================
// Resource Provider Implementation

//==============================================================================
// Resource Provider Implementation

juce::ZipFile* drumBussProcessorAudioProcessorEditor::getZipFile()
{
    static std::unique_ptr<juce::MemoryInputStream> stream;
    static std::unique_ptr<juce::ZipFile> zipFile;

    if (zipFile == nullptr)
    {
        if (BinaryData::drumbussprocessor_WebUI_zipSize > 0)
        {
            stream = std::make_unique<juce::MemoryInputStream>(
                BinaryData::drumbussprocessor_WebUI_zip,
                static_cast<size_t>(BinaryData::drumbussprocessor_WebUI_zipSize),
                false);
            zipFile = std::make_unique<juce::ZipFile>(stream.get(), false);
        }
    }
    
    return zipFile.get();
}

const char* drumBussProcessorAudioProcessorEditor::getMimeForExtension (const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { { "htm"   },  "text/html"                },
        { { "html"  },  "text/html"                },
        { { "txt"   },  "text/plain"               },
        { { "jpg"   },  "image/jpeg"               },
        { { "jpeg"  },  "image/jpeg"               },
        { { "svg"   },  "image/svg+xml"            },
        { { "ico"   },  "image/vnd.microsoft.icon" },
        { { "json"  },  "application/json"         },
        { { "png"   },  "image/png"                },
        { { "css"   },  "text/css"                 },
        { { "map"   },  "application/json"         },
        { { "js"    },  "text/javascript"          },
        { { "woff2" },  "font/woff2"               }
    };

    if (const auto it = mimeMap.find(extension.toLowerCase()); it != mimeMap.end())
        return it->second;

    jassertfalse;
    return "text/plain";
}

juce::String drumBussProcessorAudioProcessorEditor::getExtension (juce::String filename)
{
    return filename.fromLastOccurrenceOf(".", false, false);
}

std::vector<std::byte> drumBussProcessorAudioProcessorEditor::streamToVector (juce::InputStream& stream)
{
    std::vector<std::byte> result ((size_t) stream.getTotalLength());
    stream.setPosition(0);
    [[maybe_unused]] const auto bytesRead = stream.read(result.data(), result.size());
    jassert (bytesRead == (ssize_t) result.size());
    return result;
}

std::optional<juce::WebBrowserComponent::Resource> drumBussProcessorAudioProcessorEditor::getResource (const juce::String& url)
{
    const auto urlToRetrieve = url == "/" ? juce::String{ "index.html" }
                                          : url.fromFirstOccurrenceOf("/", false, false);

    // Load from embedded binary data (zip file)
    if (auto* archive = getZipFile())
    {
        if (auto* entry = archive->getEntry(urlToRetrieve))
        {
            auto stream = juce::rawToUniquePtr(archive->createStreamForEntry(*entry));
            auto data = streamToVector(*stream);
            auto mime = getMimeForExtension(getExtension(entry->filename).toLowerCase());
            return juce::WebBrowserComponent::Resource{ std::move(data), juce::String{ mime } };
        }
    }

    // Fallback: return nullopt if resource not found
    return std::nullopt;
}
