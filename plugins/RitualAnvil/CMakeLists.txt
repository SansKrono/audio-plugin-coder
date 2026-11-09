cmake_minimum_required(VERSION 3.22)

# ============================================
# RitualAnvil: drum-bus finishing chain (WebView UI)
# ============================================

# WebView backend flags differ per platform (JUCE 8):
#   Windows -> NEEDS_WEBVIEW2 (static WebView2), Linux -> NEEDS_WEB_BROWSER (webkit2gtk),
#   macOS   -> neither (system WKWebView).
if(WIN32)
    set(RANV_NEEDS_WEBVIEW2 TRUE)
    set(RANV_NEEDS_WEB_BROWSER FALSE)
    set(RANV_FORMATS VST3 Standalone)
elseif(APPLE)
    set(RANV_NEEDS_WEBVIEW2 FALSE)
    set(RANV_NEEDS_WEB_BROWSER FALSE)
    set(RANV_FORMATS VST3 AU Standalone)
else()
    set(RANV_NEEDS_WEBVIEW2 FALSE)
    set(RANV_NEEDS_WEB_BROWSER TRUE)
    set(RANV_FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(RitualAnvil
    VERSION 0.1.0
    COMPANY_NAME "SansKrono"
    PLUGIN_MANUFACTURER_CODE SnKr
    PLUGIN_CODE Ranv
    FORMATS ${RANV_FORMATS}
    PRODUCT_NAME "RitualAnvil"
    BUNDLE_ID "com.SansKrono.RitualAnvil"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics
    LV2URI "urn:sanskrono:ritualanvil"
    NEEDS_WEBVIEW2 ${RANV_NEEDS_WEBVIEW2}
    NEEDS_WEB_BROWSER ${RANV_NEEDS_WEB_BROWSER}
)

# ============================================
# Embed web UI (Source/ui/public) as a zip in BinaryData
# ============================================
set(RANV_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Source/ui/public")
set(RANV_WEB_ZIP  "${CMAKE_CURRENT_BINARY_DIR}/RitualAnvil_WebUI.zip")
file(GLOB_RECURSE RANV_WEB_FILES CONFIGURE_DEPENDS "${RANV_WEB_ROOT}/*")

add_custom_command(
    OUTPUT ${RANV_WEB_ZIP}
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${RANV_WEB_ZIP}" --format=zip "."
    WORKING_DIRECTORY "${RANV_WEB_ROOT}"
    DEPENDS ${RANV_WEB_FILES}
    COMMENT "Zipping RitualAnvil Web UI assets..."
)
add_custom_target(RitualAnvil_WebUI_Zip DEPENDS ${RANV_WEB_ZIP})

juce_add_binary_data(RitualAnvil_WebUI
    HEADER_NAME RitualAnvilWebUI.h
    NAMESPACE RitualAnvilWebUI
    SOURCES ${RANV_WEB_ZIP}
)
add_dependencies(RitualAnvil_WebUI RitualAnvil_WebUI_Zip)

# ============================================
# Sources
# ============================================
target_sources(RitualAnvil
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(RitualAnvil PRIVATE Source)

target_link_libraries(RitualAnvil
    PRIVATE
        RitualAnvil_WebUI
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

target_compile_definitions(RitualAnvil
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(RitualAnvil PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

# ============================================
# Offline DSP tests (not built by default)
#   cmake --build build --target RitualAnvil_DspTest
#   ./build/plugins/RitualAnvil/RitualAnvil_DspTest
# Links the plugin's shared-code library so the real processor is tested;
# reuses its include paths and definitions so JUCE module config matches.
# ============================================
add_executable(RitualAnvil_DspTest EXCLUDE_FROM_ALL Tests/DspTest.cpp)
target_include_directories(RitualAnvil_DspTest PRIVATE $<TARGET_PROPERTY:RitualAnvil,INCLUDE_DIRECTORIES>)
target_compile_definitions(RitualAnvil_DspTest PRIVATE $<TARGET_PROPERTY:RitualAnvil,COMPILE_DEFINITIONS>)
target_link_libraries(RitualAnvil_DspTest
    PRIVATE RitualAnvil
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)

# NOTE: no per-target XCODE_ATTRIBUTE_ARCHS. For universal builds pass
# -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" at configure time.
