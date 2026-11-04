# VaporStrings — guitar → synth transformer (WebView UI)
# Target name matches the plugin folder so scripts/build-and-install.* can find it.

# ============================================
# PLATFORM-SPECIFIC CONFIGURATION
# ============================================
set(NEEDS_WEBVIEW2 FALSE)
set(NEEDS_WEB_BROWSER FALSE)
set(AU_MAIN_TYPE "")
set(LV2_URI "")

if(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
    set(NEEDS_WEBVIEW2 TRUE)
elseif(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
    set(AU_MAIN_TYPE kAudioUnitType_Effect)
elseif(UNIX)
    set(PLUGIN_FORMATS VST3 LV2 Standalone)
    set(NEEDS_WEB_BROWSER TRUE)
    set(LV2_URI "https://github.com/noizefield/audio-plugin-coder/vaporstrings")
endif()

# ============================================
# PLUGIN DEFINITION
# ============================================
juce_add_plugin(vaporstrings
    COMPANY_NAME "Noizefield"
    PLUGIN_MANUFACTURER_CODE Nfld
    PLUGIN_CODE VpSt
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "VaporStrings"
    NEEDS_WEBVIEW2 ${NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${NEEDS_WEB_BROWSER}
    VST3_CATEGORIES Fx Modulation Reverb
    AU_MAIN_TYPE ${AU_MAIN_TYPE}
    LV2URI ${LV2_URI}
)

# ============================================
# CLAP FORMAT (opt-in) — status.json formats.clap = false
# ============================================
option(vaporstrings_ENABLE_CLAP "Build VaporStrings in CLAP format" OFF)
if(vaporstrings_ENABLE_CLAP AND COMMAND clap_juce_extensions_plugin)
    clap_juce_extensions_plugin(TARGET vaporstrings
        CLAP_ID "com.noizefield.vaporstrings"
        CLAP_FEATURES audio-effect)
endif()

# ============================================
# WEB UI (embedded) — webview-008: ONE inline index.html, no external JS/CSS.
# Regenerate with: node plugins/vaporstrings/tools/build-ui.js
# ============================================
juce_add_binary_data(vaporstrings_WebUI
    SOURCES
        Source/ui/public/index.html
)

target_sources(vaporstrings
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(vaporstrings
    PRIVATE
        Source
)

target_link_libraries(vaporstrings
    PRIVATE
        vaporstrings_WebUI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(vaporstrings
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_CURL=0
)

if(WIN32)
    target_compile_definitions(vaporstrings
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

# ============================================
# OFFLINE TEST HARNESS (not part of normal builds)
# Pitch tracker / onset checks on synthetic plucks or a DI .wav — see tests/TrackerTest.cpp
# ============================================
add_executable(vaporstrings_TrackerTest EXCLUDE_FROM_ALL tests/TrackerTest.cpp)
target_compile_features(vaporstrings_TrackerTest PRIVATE cxx_std_17)
