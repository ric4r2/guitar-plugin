cmake_minimum_required(VERSION 3.22)
project(GuitarDrive VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# Neural Amp Modeler engine (MIT license) and the Eigen version it pins (MPL-2.0), to play NAM captures.
FetchContent_Declare(nam_core
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG d3189ed482ded880c128bf312941266c4fa5ceec
    GIT_SUBMODULES ""
    SOURCE_SUBDIR not-a-cmake-project)
FetchContent_Declare(eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/bc3b39870ecb690a623a3f49149a358b95c5781d/eigen-bc3b39870ecb690a623a3f49149a358b95c5781d.tar.gz
    SOURCE_SUBDIR not-a-cmake-project)
FetchContent_MakeAvailable(nam_core eigen)

file(GLOB NAM_CORE_SOURCES "${nam_core_SOURCE_DIR}/NAM/*.cpp" "${nam_core_SOURCE_DIR}/NAM/*/*.cpp")
add_library(nam_core STATIC ${NAM_CORE_SOURCES})
target_include_directories(nam_core SYSTEM PUBLIC
    "${nam_core_SOURCE_DIR}"
    "${nam_core_SOURCE_DIR}/NAM"
    "${nam_core_SOURCE_DIR}/Dependencies/nlohmann"
    "${eigen_SOURCE_DIR}")
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_features(nam_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(nam_core PRIVATE /bigobj /W0)
endif()

juce_add_plugin(GuitarDrive
    COMPANY_NAME "Joe Garcia"
    PLUGIN_MANUFACTURER_CODE Rcrz
    PLUGIN_CODE Gdrv
    FORMATS VST3 Standalone
    PRODUCT_NAME "Joe Garcia Rig"
    ICON_BIG Assets/icon.png          # made by tools/make_icon.py from the logo
    ICON_SMALL Assets/icon-small.png
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)  # TRUE copies to Program Files\Common Files\VST3 (needs admin)

target_sources(GuitarDrive PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AmpModel.cpp Source/CabinetIR.cpp Source/IRPack.cpp Source/NamCapture.cpp Source/ChannelStrip.cpp
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
    target_sources(GuitarDriveTests PRIVATE Tests/DspTests.cpp Source/AmpModel.cpp Source/CabinetIR.cpp Source/IRPack.cpp Source/NamCapture.cpp Source/ChannelStrip.cpp Source/Effects.cpp Source/Presets.cpp)
    target_include_directories(GuitarDriveTests PRIVATE Source)
    target_compile_definitions(GuitarDriveTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(GuitarDriveTests PRIVATE
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>"
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # renders every view of the window to PNG files, to check the look without opening the program
    add_executable(GuitarDriveSnapshot Tests/Snapshot.cpp)
    target_compile_features(GuitarDriveSnapshot PRIVATE cxx_std_17)
    target_include_directories(GuitarDriveSnapshot PRIVATE Source $<TARGET_PROPERTY:GuitarDrive,INCLUDE_DIRECTORIES>)
    target_compile_definitions(GuitarDriveSnapshot PRIVATE $<TARGET_PROPERTY:GuitarDrive,COMPILE_DEFINITIONS>)
    target_link_libraries(GuitarDriveSnapshot PRIVATE GuitarDrive juce::juce_recommended_config_flags)
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
# Recorded cabinets and spring tank for your own build only: tools/pack_irs.py writes Assets/private-ir/irs.bin,
# which is never committed (most IR licenses allow use but not redistribution). Without it the built-in cabinets
# and spring are used.
set(GUITARDRIVE_ASSETS Assets/joe-garcia-logo.png)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/private-ir/irs.bin")
    list(APPEND GUITARDRIVE_ASSETS "Assets/private-ir/irs.bin")
    set(GUITARDRIVE_IR_DEFS GUITARDRIVE_IR_PACK=1)
    message(STATUS "Embedding recorded IRs: Assets/private-ir/irs.bin")
endif()
# NAM captures (tools/pack_nam.py), personal use only like the IRs
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/private-ir/nam.bin")
    list(APPEND GUITARDRIVE_ASSETS "Assets/private-ir/nam.bin")
    list(APPEND GUITARDRIVE_IR_DEFS GUITARDRIVE_NAM_PACK=1)
    message(STATUS "Embedding NAM captures: Assets/private-ir/nam.bin")
endif()
juce_add_binary_data(GuitarDriveAssets SOURCES ${GUITARDRIVE_ASSETS})
if(GUITARDRIVE_IR_DEFS)
    target_compile_definitions(GuitarDrive PRIVATE ${GUITARDRIVE_IR_DEFS})
endif()

target_link_libraries(GuitarDrive PRIVATE
    GuitarDriveAssets
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>"   # whole: model types register themselves from objects nothing calls
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
