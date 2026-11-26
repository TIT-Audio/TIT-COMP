cmake_minimum_required(VERSION 3.22)
project(TITCOMP VERSION 0.6.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# La imagen de fondo se incrusta en el plugin (BinaryData::background_jpg)
juce_add_binary_data(TITCOMPAssets SOURCES background.jpg)

# Formatos: VST3 (Windows y macOS) y AU (solo macOS). Sin AAX.
set(TITCOMP_FORMATS VST3)
if(APPLE)
    list(APPEND TITCOMP_FORMATS AU)
endif()

juce_add_plugin(TITCOMP
    COMPANY_NAME            "TIT Audio"
    PLUGIN_MANUFACTURER_CODE Titc
    PLUGIN_CODE             Tcmp
    FORMATS                 ${TITCOMP_FORMATS}
    AU_MAIN_TYPE            kAudioUnitType_Effect
    PRODUCT_NAME            "TIT COMP"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Fx Dynamics)

target_sources(TITCOMP PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    Parameters.cpp)

target_compile_definitions(TITCOMP PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TITCOMP
    PRIVATE
        TITCOMPAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
