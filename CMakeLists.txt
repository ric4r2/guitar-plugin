cmake_minimum_required(VERSION 3.22)
project(GuitarDrive VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GuitarDrive
    COMPANY_NAME "Richie"
    PLUGIN_MANUFACTURER_CODE Rcrz
    PLUGIN_CODE Gdrv
    FORMATS VST3 Standalone
    PRODUCT_NAME "GuitarDrive"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)  # TRUE copies to Program Files\Common Files\VST3 (needs admin)

target_sources(GuitarDrive PRIVATE
    Source/PluginProcessor.cpp)

target_compile_definitions(GuitarDrive PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GuitarDrive PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
