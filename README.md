# HDN Ring Modulator

[![Build](https://github.com/user1303836/hdn-ringmod/actions/workflows/build.yml/badge.svg)](https://github.com/user1303836/hdn-ringmod/actions/workflows/build.yml)

A pitch-tracking ring modulator audio plugin (VST3/AU/Standalone) built with JUCE 8.

![funny_picture](img/Screenshot%202026-02-14%20at%208.30.52 PM.png)

## What Is This?

This is just a ring modulator, with pitch tracking because there are none that are free. Traditional ring modulators use a fixed carrier frequency, which means the effect sounds different depending on what note you play -- often dissonant and hard to control.

This plugin adds **pitch tracking**. It listens to your input, detects the fundamental frequency in real time using the YIN pitch detection algorithm, and locks the modulator oscillator to that pitch. The result is a ring mod effect that tracks what you're playing, producing consistent harmonic relationships regardless of the note. The Rate Multiplier parameter lets you set the ratio between the detected pitch and the oscillator -- 1x gives you octave doubling, 2x gives you a fifth above that, and fractional values produce subharmonic content.

The plugin also has a conventional **Manual** mode where the oscillator runs at a fixed frequency, for traditional ring mod sounds.

Four oscillator waveforms are available (sine, triangle, square, saw), each producing a different harmonic character. Square and saw use PolyBLEP anti-aliasing to reduce digital artifacts.

## Requirements

- CMake 3.25+
- C++20 compiler
- **macOS**: Xcode command line tools
- **Windows**: Visual Studio 2022 (MSVC)

## Building

Clone with submodules (JUCE is pinned as a git submodule):

```bash
git clone --recursive https://github.com/user1303836/hdn-ringmod.git
cd hdn-ringmod
```

### macOS

Requires Xcode command line tools (`xcode-select --install`).

Using the default Makefile generator:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

For faster builds with Ninja (`brew install ninja`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --config Release
```

To build a universal binary (arm64 + x86_64):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

The built plugins will be in:

- `build/HdnRingmod_artefacts/Release/VST3/HDN Ring Modulator.vst3`
- `build/HdnRingmod_artefacts/Release/AU/HDN Ring Modulator.component`
- `build/HdnRingmod_artefacts/Release/Standalone/HDN Ring Modulator.app`

To install, copy the `.vst3` to `~/Library/Audio/Plug-Ins/VST3/` or the `.component` to `~/Library/Audio/Plug-Ins/Components/`.

### Windows

Requires Visual Studio 2022 with the "Desktop development with C++" workload.

From a **Developer Command Prompt for VS 2022** (or using `vcvarsall.bat`):

```cmd
cmake -B build
cmake --build build --config Release
```

The built plugin will be in `build\HdnRingmod_artefacts\Release\VST3\HDN Ring Modulator.vst3`.

To install, copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\`.

### Running Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHDN_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Parameters

| Parameter       | Range                          | Default     | Description                              |
| --------------- | ------------------------------ | ----------- | ---------------------------------------- |
| Mix             | 0 - 100%                       | 50%         | Dry/wet blend                            |
| Rate Multiplier | 0.1 - 8.0x                     | 1.0x        | Multiplier applied to tracked pitch      |
| Manual Rate     | 20 - 5000 Hz                   | 440 Hz      | Fixed oscillator frequency (Manual mode) |
| Mode            | Pitch Track / Manual           | Pitch Track | Pitch source selection                   |
| Smoothing       | 0 - 100%                       | 50%         | Pitch tracking smoothing amount          |
| Sensitivity     | 0 - 100%                       | 50%         | Minimum confidence for accepting pitch updates; higher values require stronger detections |
| Waveform        | Sine / Triangle / Square / Saw | Sine        | Ring modulator oscillator shape          |

## How It Works

In **Pitch Track** mode, the plugin detects the pitch of the incoming audio using the YIN algorithm, then ring-modulates the signal with an oscillator locked to that pitch (multiplied by the Rate Multiplier). The effect stays dry only until the tracker has a valid pitch, then follows the tracked carrier directly.

The tracker uses short analysis windows for normal guitar notes and runs longer analyses for low or uncertain pitches, with slower background audits for octave errors. It supports fundamentals down to 20 Hz without imposing the low-frequency analysis rate on the common path. Acquisition can take roughly 110 ms near 20 Hz because one cycle alone is 50 ms.

In **Manual** mode, the oscillator runs at a fixed frequency set by the Manual Rate knob.

## License

GPLv3. See [LICENSE](LICENSE).
