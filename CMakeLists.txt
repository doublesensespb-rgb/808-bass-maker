cmake_minimum_required(VERSION 3.22)
project(BassMaker VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(JUCE)

# NEVER change PLUGIN_MANUFACTURER_CODE, PLUGIN_CODE, BUNDLE_ID or PRODUCT_NAME after release:
# the DAW would see a different plugin and saved projects would break.
juce_add_plugin(BassMaker
    COMPANY_NAME "Thematic"
    PLUGIN_MANUFACTURER_CODE Thmt
    PLUGIN_CODE B808
    FORMATS VST3 AU Standalone
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    BUNDLE_ID com.thematic.808bassmaker
    PRODUCT_NAME "808 BASS MAKER"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(BassMaker PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BassMaker PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(BassMaker PRIVATE /utf-8)
endif()

target_link_libraries(BassMaker
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offscreen renderer used by CI (and locally) to preview the UI
juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
target_sources(Snapshot PRIVATE
    tools/Snapshot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_compile_definitions(Snapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1)
if(UNIX AND NOT APPLE)
    target_compile_definitions(Snapshot PRIVATE JUCE_ALSA=0 JUCE_JACK=0)
endif()
if(MSVC)
    target_compile_options(Snapshot PRIVATE /utf-8)
endif()
target_link_libraries(Snapshot PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags)
