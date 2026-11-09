cmake_minimum_required(VERSION 3.22)

# ============================================
# RitualComp76: 1176-style FET compressor (WebView UI)
# ============================================

# WebView backend flags differ per platform (JUCE 8):
#   Windows -> NEEDS_WEBVIEW2 (static WebView2), Linux -> NEEDS_WEB_BROWSER (webkit2gtk),
#   macOS   -> neither (system WKWebView).
if(WIN32)
    set(RC76_NEEDS_WEBVIEW2 TRUE)
    set(RC76_NEEDS_WEB_BROWSER FALSE)
    set(RC76_FORMATS VST3 Standalone)
elseif(APPLE)
    set(RC76_NEEDS_WEBVIEW2 FALSE)
    set(RC76_NEEDS_WEB_BROWSER FALSE)
    set(RC76_FORMATS VST3 AU Standalone)
else()
    set(RC76_NEEDS_WEBVIEW2 FALSE)
    set(RC76_NEEDS_WEB_BROWSER TRUE)
    set(RC76_FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(RitualComp76
    VERSION 0.0.1
    COMPANY_NAME "SansKrono"
    PLUGIN_MANUFACTURER_CODE SnKr
    PLUGIN_CODE Rc76
    FORMATS ${RC76_FORMATS}
    PRODUCT_NAME "RitualComp76"
    BUNDLE_ID "com.SansKrono.RitualComp76"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics
    LV2URI "urn:sanskrono:ritualcomp76"
    NEEDS_WEBVIEW2 ${RC76_NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${RC76_NEEDS_WEB_BROWSER}
)

# ============================================
# Embed web UI (Source/ui/public) as a zip in BinaryData
# ============================================
set(RC76_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Source/ui/public")
set(RC76_WEB_ZIP  "${CMAKE_CURRENT_BINARY_DIR}/RitualComp76_WebUI.zip")
file(GLOB_RECURSE RC76_WEB_FILES CONFIGURE_DEPENDS "${RC76_WEB_ROOT}/*")

add_custom_command(
    OUTPUT ${RC76_WEB_ZIP}
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${RC76_WEB_ZIP}" --format=zip "."
    WORKING_DIRECTORY "${RC76_WEB_ROOT}"
    DEPENDS ${RC76_WEB_FILES}
    COMMENT "Zipping RitualComp76 Web UI assets..."
)
add_custom_target(RitualComp76_WebUI_Zip DEPENDS ${RC76_WEB_ZIP})

juce_add_binary_data(RitualComp76_WebUI
    HEADER_NAME RitualComp76WebUI.h
    NAMESPACE RitualComp76WebUI
    SOURCES ${RC76_WEB_ZIP}
)
add_dependencies(RitualComp76_WebUI RitualComp76_WebUI_Zip)

# ============================================
# Sources
# ============================================
target_sources(RitualComp76
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/FetCompressor.cpp
)

target_include_directories(RitualComp76 PRIVATE Source)

target_link_libraries(RitualComp76
    PRIVATE
        RitualComp76_WebUI
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(RitualComp76
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(RitualComp76 PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

# ============================================
# Offline DSP tests (not built by default)
#   cmake --build build --target RitualComp76_DspTest
#   ./build/plugins/RitualComp76/RitualComp76_DspTest
# Links the plugin's shared-code library so the real processor is tested;
# reuses its include paths and definitions so JUCE module config matches.
# ============================================
add_executable(RitualComp76_DspTest EXCLUDE_FROM_ALL Tests/DspTest.cpp)
target_include_directories(RitualComp76_DspTest PRIVATE $<TARGET_PROPERTY:RitualComp76,INCLUDE_DIRECTORIES>)
target_compile_definitions(RitualComp76_DspTest PRIVATE $<TARGET_PROPERTY:RitualComp76,COMPILE_DEFINITIONS>)
target_link_libraries(RitualComp76_DspTest
    PRIVATE RitualComp76
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)

# NOTE: no per-target XCODE_ATTRIBUTE_ARCHS. For universal builds pass
# -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" at configure time.
