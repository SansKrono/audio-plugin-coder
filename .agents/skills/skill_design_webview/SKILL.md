---
name: juce-webview-windows
description: Quick-start guide for building JUCE 9 audio plugins with WebView2 UIs on Windows. Covers essential setup, critical member ordering, and step-by-step implementation workflow.
---

# JUCE 9 WebView Plugin - Quick Start Guide

**Platform:** Windows 11 | JUCE 9 | WebView2 | CMake

---

## 🎯 Quick Overview

Build audio plugin UIs using modern web technologies (HTML/CSS/JavaScript) instead of C++ JUCE components.

**Benefits:**
- Fast iteration with hot reloading
- Use familiar web frameworks (React, Vue, or vanilla JS)
- Modern UI capabilities (CSS animations, flexbox, gradients)
- Team-friendly (frontend devs can work without C++ knowledge)
- GPU acceleration via WebGL

**Trade-offs:**
- ~100MB additional memory footprint
- Windows 11 WebView2 dependency
- Different performance characteristics than native C++

---

## 📐 UI Layout & Scaling (read before any layout work)

Canonical rules live in **`.agents/rules/ui-layout-system.md`**. In short:

- Geometry is authored on a grid in `rem` (1 rem = 1 cell); scaling is pure CSS
  via `html { font-size: min(100vw/cols, 100vh/rows) }` — **never** CSS `zoom`,
  `transform: scale` wrappers, or JS-computed scale factors.
- `Design/ui-map.json` is the contract; `index.html` must match it 1:1
  (`id="ctl-<param>"`). Vendor `templates/webview/apc-ui/` →
  `Source/ui/public/apc-ui/`, don't hand-roll components.
- Resizable plugins use an **in-page** `.apc-grip` (the JUCE
  `ResizableCornerComponent` cannot paint over a heavyweight webview), wired to
  the `resizeGrip` native function; aspect is locked via
  `setFixedAspectRatio` **and** re-asserted in `resized()`.
- Set `WinWebView2{}.withBackgroundColour(<plate colour>)` to kill drag-flash.
- Gate: `node bin/apc.js validate ui --plugin <Name>` must pass before build.

---

## 🔴 CRITICAL: Member Declaration Order (PREVENTS DAW CRASHES)

**⚠️ #1 CAUSE OF WEBVIEW PLUGIN CRASHES - MUST FOLLOW**

### The Rule

C++ destroys members in **REVERSE order of declaration**. WebView references relays, so relays must be declared FIRST to be destroyed LAST.

### Correct Pattern (PluginEditor.h)

```cpp
private:
    YourAudioProcessor& audioProcessor;

    // ═══════════════════════════════════════════════════════════════
    // CRITICAL: Destruction Order = Reverse of Declaration
    // Order: Relays → WebView → Attachments
    // ═══════════════════════════════════════════════════════════════

    // 1. RELAYS FIRST (destroyed last)
    juce::WebSliderRelay gainRelay { "GAIN" };
    juce::WebSliderRelay frequencyRelay { "FREQUENCY" };

    // 2. WEBVIEW SECOND (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. ATTACHMENTS LAST (destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> frequencyAttachment;
```

**Wrong Order → DAW Crash on Unload**

See: [`.agents/troubleshooting/resolutions/webview-member-order-crash.md`](../../troubleshooting/resolutions/webview-member-order-crash.md)

---

## 📋 Step-by-Step Implementation

### Step 1: Create Web UI Files

```
plugins/YourPlugin/
└── Source/
    └── ui/
        └── public/
            ├── index.html
            └── js/
                └── index.js
```

> **CRITICAL (webview-008):** ES6 modules fail in JUCE WebView - there must be NO separate .js files and no external scripts. Everything goes inline into ONE <script> block in index.html.

### Step 2: Write index.html

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>My Plugin</title>
  <!-- webview-008: ALL JavaScript must be INLINE - external scripts fail silently -->
  <style>
    body {
      margin: 0;
      padding: 20px;
      background: #1a1a2e;
      color: #e0e0e0;
      font-family: system-ui, sans-serif;
    }
    input[type="range"] { width: 100%; }
  </style>
</head>
<body>
  <h1>My Plugin</h1>
  <div>
    <label for="gainSlider">Gain</label>
    <input type="range" id="gainSlider" min="0" max="1" step="0.01" value="0.5">
    <span id="gainValue">0.5</span>
  </div>
</body>
</html>
```

### Step 3: Inline JavaScript (inside index.html)

> **CRITICAL (webview-008):** ES6 modules do NOT work in JUCE WebView. Never create separate `.js` files. Inline ALL JavaScript - including the JUCE frontend library exposed as `window.Juce` - in ONE `<script>` block.

```javascript
const Juce = window.Juce; // JUCE library INLINED above - never use import/export (webview-008)

document.addEventListener("DOMContentLoaded", () => {
    // Get parameter state from C++
    const gainState = Juce.getSliderState("GAIN");

    const gainSlider = document.getElementById("gainSlider");
    const gainValue = document.getElementById("gainValue");

    // User interaction → Update C++
    gainSlider.addEventListener("mousedown", () => gainState.sliderDragStarted());
    gainSlider.addEventListener("mouseup", () => gainState.sliderDragEnded());
    gainSlider.addEventListener("input", () => {
        gainState.setNormalisedValue(gainSlider.value);
        gainValue.textContent = gainSlider.value;
    });

    // C++ automation → Update UI
    gainState.valueChangedEvent.addListener(() => {
        const value = gainState.getNormalisedValue();
        gainSlider.value = value;
        gainValue.textContent = value.toFixed(2);
    });
});
```

### Step 4: Configure CMakeLists.txt

```cmake
# Embed web files into binary
juce_add_binary_data(YourPlugin_WebUI
    SOURCES
        Source/ui/public/index.html
        # NO JS FILES - all JavaScript must be inline in index.html (webview-008)
)

# Plugin definition
juce_add_plugin(YourPlugin
    FORMATS VST3 Standalone
    PRODUCT_NAME "Your Plugin"
    NEEDS_WEBVIEW2 TRUE
)

# Link binary data
target_link_libraries(YourPlugin
    PRIVATE
        YourPlugin_WebUI
        juce::juce_gui_extra
        # ... other modules
)

# WebView2 definitions
target_compile_definitions(YourPlugin
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
)
```

### Step 4b: macOS / Linux (cross-platform WebView)

The steps above target Windows/WebView2. For a plugin that must also build on macOS (WKWebView) and Linux (webkit2gtk) — see `plugins/RitualComp76` and `plugins/RitualAnvil` for working examples:

- **Embed the UI as one zip**, not loose files: avoids BinaryData name collisions and keeps the folder structure (`css/`, `js/juce/`):
  ```cmake
  set(WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Source/ui/public")
  set(WEB_ZIP  "${CMAKE_CURRENT_BINARY_DIR}/YourPlugin_WebUI.zip")
  file(GLOB_RECURSE WEB_FILES CONFIGURE_DEPENDS "${WEB_ROOT}/*")
  add_custom_command(OUTPUT ${WEB_ZIP}
      COMMAND ${CMAKE_COMMAND} -E tar "cf" "${WEB_ZIP}" --format=zip "."
      WORKING_DIRECTORY "${WEB_ROOT}" DEPENDS ${WEB_FILES})
  add_custom_target(YourPlugin_WebUI_Zip DEPENDS ${WEB_ZIP})
  juce_add_binary_data(YourPlugin_WebUI HEADER_NAME YourPluginWebUI.h NAMESPACE YourPluginWebUI SOURCES ${WEB_ZIP})
  add_dependencies(YourPlugin_WebUI YourPlugin_WebUI_Zip)
  ```
  The resource provider then serves entries from a `juce::ZipFile` over that blob; `cmake -E tar` may store entries as `./path`, so try both spellings.
- **Platform flags in `juce_add_plugin`:** Windows `NEEDS_WEBVIEW2 TRUE`; Linux `NEEDS_WEB_BROWSER TRUE`; macOS neither. `JUCE_WEB_BROWSER=1` everywhere; `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` only under `if(WIN32)`; `JUCE_USE_CURL=0` on macOS/Linux.
- **Backend only on Windows:** wrap `.withBackend(webview2)` and `.withWinWebView2Options(...)` in `#if JUCE_WINDOWS`; macOS/Linux use the default backend.
- **Formats:** add `AU` on macOS, `LV2` on Linux.

### Step 5: Create ParameterIDs.hpp

```cpp
#pragma once

namespace ParameterIDs {
    constexpr char GAIN[] = "GAIN";
    constexpr char FREQUENCY[] = "FREQUENCY";
}
```

### Step 6: Write PluginEditor.h

```cpp
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ParameterIDs.hpp"

class YourPluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit YourPluginEditor(YourAudioProcessor&);
    ~YourPluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    YourAudioProcessor& audioProcessor;

    // CRITICAL: Relays → WebView → Attachments
    juce::WebSliderRelay gainRelay { ParameterIDs::GAIN };
    juce::WebSliderRelay frequencyRelay { ParameterIDs::FREQUENCY };

    std::unique_ptr<juce::WebBrowserComponent> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> frequencyAttachment;

    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);
    static const char* getMimeForExtension(const juce::String& extension);
    static juce::String getExtension(juce::String filename);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YourPluginEditor)
};
```

### Step 7: Write PluginEditor.cpp

```cpp
#include "PluginEditor.h"
#include "BinaryData.h"

YourPluginEditor::YourPluginEditor(YourAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(600, 400);

    // Create WebView with relay references
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options()
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withUserDataFolder(juce::File::getSpecialLocation(
                        juce::File::SpecialLocationType::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withOptionsFrom(gainRelay)
            .withOptionsFrom(frequencyRelay)
            .withResourceProvider([this](const auto& url) {
                return getResource(url);
            })
    );

    addAndMakeVisible(*webView);

    // Create attachments (AFTER webView)
    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *audioProcessor.getAPVTS().getParameter(ParameterIDs::GAIN),
        gainRelay,
        nullptr
    );

    frequencyAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *audioProcessor.getAPVTS().getParameter(ParameterIDs::FREQUENCY),
        frequencyRelay,
        nullptr
    );

    // Load UI
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
}

YourPluginEditor::~YourPluginEditor() {}

void YourPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void YourPluginEditor::resized()
{
    webView->setBounds(getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource> YourPluginEditor::getResource(const juce::String& url)
{
    const auto urlToRetrieve = url == "/" ? juce::String{ "index.html" }
                                          : url.fromFirstOccurrenceOf("/", false, false);

    // Try to find resource in BinaryData
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const char* resourceName = BinaryData::namedResourceList[i];
        const char* originalFilename = BinaryData::getNamedResourceOriginalFilename(resourceName);

        if (originalFilename != nullptr && juce::String(originalFilename).endsWith(urlToRetrieve))
        {
            int dataSize = 0;
            const char* data = BinaryData::getNamedResource(resourceName, dataSize);

            if (data != nullptr && dataSize > 0)
            {
                std::vector<std::byte> byteData((size_t)dataSize);
                std::memcpy(byteData.data(), data, (size_t)dataSize);
                auto mime = getMimeForExtension(getExtension(urlToRetrieve).toLowerCase());
                return juce::WebBrowserComponent::Resource{ std::move(byteData), juce::String{ mime } };
            }
        }
    }

    return std::nullopt;
}

const char* YourPluginEditor::getMimeForExtension(const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap =
    {
        { { "html" }, "text/html" },
        { { "css"  }, "text/css" },
        { { "js"   }, "text/javascript" },
        { { "json" }, "application/json" },
        { { "png"  }, "image/png" },
        { { "jpg"  }, "image/jpeg" },
        { { "svg"  }, "image/svg+xml" }
    };

    if (const auto it = mimeMap.find(extension.toLowerCase()); it != mimeMap.end())
        return it->second;

    return "text/plain";
}

juce::String YourPluginEditor::getExtension(juce::String filename)
{
    return filename.fromLastOccurrenceOf(".", false, false);
}
```

### Step 8: Build

```powershell
.\scripts\build-and-install.ps1 -PluginName YourPlugin
```

### Step 9: Test

1. Load plugin in DAW
2. Open plugin window → UI should display
3. Move sliders → parameters should update
4. Automate in DAW → UI should update
5. **Close window → should NOT crash**
6. Unload plugin → should NOT crash

---

## ✅ Validation Checklist

Before considering your WebView plugin complete:

### Code Structure
- [ ] Member order: Relays → WebView → Attachments
- [ ] Destruction order comment added to header
- [ ] All relays are direct members (not `unique_ptr`)
- [ ] WebView and attachments are `unique_ptr`

### WebView Setup
- [ ] `.withBackend(webview2)` specified
- [ ] `.withWinWebView2Options(...withUserDataFolder(...))` provided
- [ ] `.withNativeIntegrationEnabled()` included
- [ ] All relays registered: `.withOptionsFrom(relay)` for each
- [ ] Resource provider implemented
- [ ] `webView->goToURL(...)` called

### CMakeLists.txt
- [ ] All web files in `juce_add_binary_data()`
- [ ] `NEEDS_WEBVIEW2 TRUE` set
- [ ] Binary data target linked
- [ ] `JUCE_WEB_BROWSER=1` defined
- [ ] `JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1` defined

### Resource Provider
- [ ] Handles "/" → "index.html" mapping
- [ ] Iterates BinaryData when direct lookup fails
- [ ] Correct MIME types returned
- [ ] Returns `std::nullopt` for missing resources

### JavaScript
- [ ] Parameter IDs match C++ exactly
- [ ] `sliderDragStarted()` / `sliderDragEnded()` called
- [ ] `valueChangedEvent.addListener()` used for automation

### Testing
- [ ] Plugin loads without errors
- [ ] UI displays correctly
- [ ] Parameters work (user interaction)
- [ ] Automation works (DAW → UI updates)
- [ ] **Window closes without crash**
- [ ] **Plugin unloads without crash**
- [ ] Multiple instances work

---

## ⚠️ Common Mistakes

### ❌ Wrong Member Order
```cpp
// WRONG - Crashes on unload!
std::unique_ptr<juce::WebBrowserComponent> webView;
juce::WebSliderRelay relay { "PARAM" };
```

### ❌ Missing .withOptionsFrom()
```cpp
// WRONG - Parameter binding won't work!
webView = std::make_unique<juce::WebBrowserComponent>(
    Options().withBackend(webview2)
    // Missing: .withOptionsFrom(gainRelay)
);
```

### ❌ Wrong MIME Type
```cpp
// WRONG - JS files won't execute!
return Resource{ data, "text/html" }; // For a .js file!
```

### ❌ Creating Attachments Before WebView
```cpp
// WRONG - Order matters!
gainAttachment = std::make_unique<...>(...);  // Too early
webView = std::make_unique<...>(...);         // Too late
```

### ❌ Embedding Separate JS Files
```cmake
# WRONG - JS files are never loaded (ES6 modules fail - webview-008)!
juce_add_binary_data(Plugin_WebUI
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/index.js   # NOT loaded - must be inlined instead!
)
```

---

## 📚 Additional Resources

For detailed technical information, see these canonical files
(paths relative to the repo root; from this directory prefix them with `../../`):

- **[WebView Framework Analysis](../../WEBVIEW_FRAMEWORK_ANALYSIS.md)** - WebView2 internals, architecture
- **[Native Function API](../../troubleshooting/resolutions/webview-005-juce-native-function-api.md)** - Frontend-backend events, native functions
- **[Resource Providers](../../troubleshooting/resolutions/webview-resource-provider-not-called.md)** - Detailed resource serving patterns
- **[Troubleshooting (this skill)](KNOWN-ISSUES-AND-SOLUTIONS.md)** - Debugging, performance, common issues
- **[Known Issues DB](../../troubleshooting/known-issues.yaml)** - webview-001, webview-002

---

## 🔗 Related Documentation

- **Troubleshooting:** `.agents/troubleshooting/resolutions/webview-member-order-crash.md`
- **Templates:** `templates/webview/`
- **Working Example:** `plugins/CloudWash/Source/ui/` (if present in your checkout)
- **Known Issues:** `.agents/troubleshooting/known-issues.yaml` (webview-001, webview-002)

---

**Document Version:** 2.0 (Streamlined)
**Last Updated:** 2026-01-24
**Status:** Production Ready
