cmake_minimum_required(VERSION 3.15)

# Plugin definition with WebView2 support
juce_add_plugin(drumbussprocessor
    COMPANY_NAME "Noizefield"
    PLUGIN_MANUFACTURER_CODE Noiz
    PLUGIN_CODE dbPr
    FORMATS VST3 Standalone
    PRODUCT_NAME "drumBussProcessor"
    NEEDS_WEBVIEW2 TRUE
)

# CRITICAL: Embed web UI files into binary
# We create a zip file of the public folder to avoid file name collisions in BinaryData
# and to match the C++ code's expectation of a zip file.

set(WEB_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Source/ui")
set(WEB_UI_ZIP "${CMAKE_CURRENT_BINARY_DIR}/drumbussprocessor_WebUI.zip")

# Create the zip file command
add_custom_command(
    OUTPUT ${WEB_UI_ZIP}
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${WEB_UI_ZIP}" --format=zip "."
    WORKING_DIRECTORY "${WEB_UI_ROOT}/public"
    DEPENDS 
        "${WEB_UI_ROOT}/public/index.html"
        "${WEB_UI_ROOT}/public/js/index.js"
        "${WEB_UI_ROOT}/public/js/juce/index.js"
    COMMENT "Zipping Web UI assets..."
)

# Create a target for the zip file generation to ensure it runs
add_custom_target(drumbussprocessor_WebUI_Zip DEPENDS ${WEB_UI_ZIP})

# Embed the zip file
juce_add_binary_data(drumbussprocessor_WebUI
    SOURCES
        ${WEB_UI_ZIP}
)

# Ensure the zip is created before compiling the binary data
add_dependencies(drumbussprocessor_WebUI drumbussprocessor_WebUI_Zip)

# Source files
target_sources(drumbussprocessor
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Include paths
target_include_directories(drumbussprocessor
    PRIVATE
        Source
        "${CMAKE_CURRENT_BINARY_DIR}/drumbussprocessor_artefacts/JuceLibraryCode"
)

# Required JUCE modules
target_link_libraries(drumbussprocessor
    PRIVATE
        drumbussprocessor_WebUI
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

# CRITICAL: WebView2 compile definitions
target_compile_definitions(drumbussprocessor
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
    target_compile_definitions(drumbussprocessor
        PUBLIC
            JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
elseif(APPLE)
    target_compile_definitions(drumbussprocessor
        PUBLIC
            JUCE_USE_CURL=0
    )
    
    set_target_properties(drumbussprocessor PROPERTIES
        XCODE_ATTRIBUTE_ARCHS "x86_64;arm64"
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH NO
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(drumbussprocessor
        PUBLIC
            JUCE_USE_CURL=0
            JUCE_JACK=1
            JUCE_WEB_BROWSER=1
    )
endif()
