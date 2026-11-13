# Joe Garcia Rig

A guitar amp and pedalboard plugin (VST3) and standalone app, built with [JUCE](https://juce.com) and CMake.

- Tweed-style amp with gate, transposer, tube warmth, bass/mid/treble, plate or spring reverb, and master
- Pedals: wah (manual and auto), compressor, tube screamer, overdrive, phaser, chorus, tremolo, delay, digital reverb
- 8-band graphic EQ
- Mono mode, or stereo mode with a full separate chain for each channel (pan, solo, mute)
- Chromatic tuner, metronome with tap tempo, presets
- Input and output volume with level meters

## Build (Windows)

You need Git, CMake and the Visual Studio C++ build tools. JUCE is downloaded automatically.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The standalone app and the VST3 end up under `build/GuitarDrive_artefacts/Release/`.

### ASIO (optional, for low latency in the standalone app)

Download Steinberg's ASIO SDK yourself (it is not included here) and point CMake at its `ASIOSDK` folder:

```
cmake -S . -B build -DASIO_SDK_DIR="C:/path/to/ASIOSDK"
```

Check Steinberg's license before sharing binaries built with it.

### Tests

The offline DSP tests (pitch shifter, gate, EQ, metronome, presets and more) are a separate target:

```
cmake -S . -B build -DGUITARDRIVE_BUILD_TESTS=ON
cmake --build build --config Release --target GuitarDriveTests
build/GuitarDriveTests_artefacts/Release/GuitarDriveTests.exe
```

## Layout

- `Source/` the plugin: `ChannelStrip` (per-channel signal chain), `Effects`, `PlateReverb`, `SpringReverb`, `Presets`, the editor and widgets
- `Tests/` the DSP tests
- `Assets/` the logo (embedded in the program)
