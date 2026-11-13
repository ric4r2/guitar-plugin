cmake_minimum_required(VERSION 3.22)
project(GuitarDrive VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GuitarDrive
    COMPANY_NAME "Joe Garcia"
    PLUGIN_MANUFACTURER_CODE Rcrz
    PLUGIN_CODE Gdrv
    FORMATS VST3 Standalone
    PRODUCT_NAME "Joe Garcia Rig"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)  # TRUE copies to Program Files\Common Files\VST3 (needs admin)

target_sources(GuitarDrive PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AmpModel.cpp Source/CabinetIR.cpp Source/ChannelStrip.cpp
    Source/Effects.cpp
    Source/Presets.cpp)

# Standalone app: JUCE's stock app, except the window always opens centred.
target_sources(GuitarDrive_Standalone PRIVATE Source/StandaloneApp.cpp)
target_compile_definitions(GuitarDrive_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

target_compile_definitions(GuitarDrive PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Offline DSP tests: cmake -DGUITARDRIVE_BUILD_TESTS=ON, then build the GuitarDriveTests target and run it.
option(GUITARDRIVE_BUILD_TESTS "Build the offline DSP tests" OFF)
if(GUITARDRIVE_BUILD_TESTS)
    juce_add_console_app(GuitarDriveTests PRODUCT_NAME "GuitarDriveTests")
    target_sources(GuitarDriveTests PRIVATE Tests/DspTests.cpp Source/AmpModel.cpp Source/CabinetIR.cpp Source/ChannelStrip.cpp Source/Effects.cpp Source/Presets.cpp)
    target_include_directories(GuitarDriveTests PRIVATE Source)
    target_compile_definitions(GuitarDriveTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GuitarDriveTests PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# Optional ASIO for the Standalone app: -DASIO_SDK_DIR=<path to ASIOSDK folder containing common/>
# The SDK is not redistributable here, so keep it outside the repo.
set(ASIO_SDK_DIR "" CACHE PATH "Steinberg ASIO SDK root (the folder containing common/)")
if(ASIO_SDK_DIR AND EXISTS "${ASIO_SDK_DIR}/common/asio.h")
    message(STATUS "ASIO enabled: ${ASIO_SDK_DIR}")
    target_compile_definitions(GuitarDrive PUBLIC JUCE_ASIO=1)
    target_include_directories(GuitarDrive PRIVATE "${ASIO_SDK_DIR}/common")
endif()

# The logo is an image file embedded in the program (replace Assets/joe-garcia-logo.png to change it).
# Recorded cabinet impulse responses for your own build only. Files in Assets/private-ir/ are never committed (most
# free IRs may be used but not redistributed). When present they replace the built-in cabinets.
set(GUITARDRIVE_ASSETS Assets/joe-garcia-logo.png)
foreach(ir fender tworock)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/private-ir/${ir}.wav")
        list(APPEND GUITARDRIVE_ASSETS "Assets/private-ir/${ir}.wav")
        string(TOUPPER ${ir} IR_UPPER)
        list(APPEND GUITARDRIVE_IR_DEFS "GUITARDRIVE_IR_${IR_UPPER}=1")
        message(STATUS "Using private cabinet IR: Assets/private-ir/${ir}.wav")
    endif()
endforeach()
juce_add_binary_data(GuitarDriveAssets SOURCES ${GUITARDRIVE_ASSETS})
if(GUITARDRIVE_IR_DEFS)
    target_compile_definitions(GuitarDrive PRIVATE ${GUITARDRIVE_IR_DEFS})
endif()

target_link_libraries(GuitarDrive PRIVATE
    GuitarDriveAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
