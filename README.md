# eqMax

eqMax is a macOS system audio equalizer and per-application volume mixer. It captures application audio with Core Audio process taps, applies stereo EQ and effects, and plays the result through your chosen output device. It uses C++20, Qt 6 Widgets, and Apple's Accelerate framework.

The interface has collapsible sections for Output, Volume, Equalizers, Effects, Applications, and Analyzer. A menu bar icon keeps the controls available when the window is closed.

## Features

### Equalizer

All modes offer gains from **−12 dB to +12 dB**, an editable response graph, and a Reset button that flattens the EQ bands.

| Mode | Controls |
| --- | --- |
| Basic | Bass, Mid, and Treble knobs at 100 Hz, 1 kHz, and 10 kHz |
| Advanced | 10 fixed bands from 32 Hz to 16 kHz |
| Expert | 32 fixed bands from 20 Hz to 20 kHz |

Drag graph points or adjust the knobs and band faders to change gain. Band frequencies and filter widths are fixed. Switching EQ modes resets the band gains; loading a preset restores its saved mode and gains.

### Volume and effects

- **Master gain:** −60 dB to +24 dB, with left/right balance.
- **Bass Boost:** a 90 Hz low-shelf boost from 0 to +12 dB.
- **Clarity:** an 8 kHz high-shelf boost from 0 to +12 dB.
- **Stereo Width:** mid-side processing from 0% (mono), through 100% (original stereo), to 200% (wider stereo).
- **Limiter:** an optional stereo-linked peak limiter, enabled by default, with a 0.98 ceiling and a 50 ms release.

The header power button and **⌘B** bypass the EQ and the entire effects chain, including master gain, balance, and limiting. Per-application volume and mute remain active while bypassed.

### Per-application mixer

The Applications section lists apps connected to the audio server, groups helper processes under their owning app, and indicates which apps are playing. Each app has a **0–200%** volume slider and a mute button. Customized levels and mute states are saved between launches.

### Presets and saved settings

There are 11 factory presets: Flat, Bass Booster, Treble Booster, Rock, Pop, Jazz, Electronic, Vocal Booster, Classical, Acoustic, and Gaming / Movies.

Use **Save** or **⌘S** to save the current EQ, gain, balance, width, boost, and limiter settings as a user preset. The **•••** menu imports presets, exports the current settings, and deletes user presets. Import loads a sound for auditioning; use Save to add it to your library. Editing a sound changes the preset selector to **Custom**.

User presets are validated JSON stored in `.eqpreset` files under `~/.config/eqMax/presets/`, with atomic writes. The original line-based eqMax preset format can also be read. Factory presets cannot be overwritten or deleted.

The app also restores the current sound, bypass state, selected output, window geometry, and collapsed sections. Test signals start off on every launch.

### Analyzer and test signals

The Analyzer section shows a 64-band FFT spectrum with peak hold and stereo level meters with clipping indicators. These display audio after EQ and effects, using custom QPainter widgets.

**Output → Setup** provides a 1 kHz sine wave, a noise generator, and a sine sweep, plus controls to open audio permissions and restart routing. Test signals mix with captured audio and pass through the EQ and effects. The noise option is labeled “Pink Noise” in the UI; its current implementation produces unfiltered pseudorandom noise.

## Requirements

- **macOS 14.2 or newer** for Core Audio process taps. System audio routing is implemented for macOS.
- Permission to record system audio.
- For the Nix workflow below: **Nix with flakes enabled**. The flake supplies CMake, Ninja, Qt 6, and the Apple SDK.
- For a manually configured build: a C++20/Objective-C++ toolchain, a macOS SDK with process-tap APIs, CMake 3.20+, and Qt 6 with Core, Gui, Widgets, and Test components.

eqMax uses process taps and a private aggregate device created at runtime; no separate virtual audio driver or kernel extension needs to be installed. Processing is stereo, including when the selected output has more channels.

## Build and run

Clone the repository and enter its development shell:

```bash
git clone git@github.com:MilanMarocchi/eqMax.git
cd eqMax
nix develop
```

Run these commands inside that shell:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/eqMax.app/Contents/MacOS/eqMax
```

The development app bundle is written to `build/eqMax.app`. CMake ad-hoc signs the bundle when `codesign` is available.

### Install with Nix

From the repository directory, build the Nix package:

```bash
nix build
```

The packaged app is available at `result/Applications/eqMax.app`. The included installer builds the package and copies it to `~/Applications/eqMax.app`, replacing an existing copy there:

```bash
nix run .#install
open ~/Applications/eqMax.app
```

The Nix package uses dependencies in the Nix store; the copied bundle should not be treated as a standalone distribution for machines without those dependencies.

## First launch and audio routing

1. Launch eqMax and allow system audio recording when macOS prompts.
2. Choose **System Default Output** to follow the macOS output, or select a specific output device in the Output section.
3. Play audio from another application, then adjust its volume or the shared EQ and effects. With processing enabled and routing active, the header displays the output sample rate.

If the header shows **Not routing**, or captured audio is silent, use **Output → Setup → System Audio Recording permission…** to open the relevant privacy settings and enable eqMax. Then use **Restart audio routing**, or quit and relaunch eqMax. The in-app permission help points to **Privacy & Security → Screen & System Audio Recording → System Audio Recording Only**; the wording can vary with the macOS version.

The output list refreshes as devices connect or disconnect. eqMax excludes its own playback from capture to avoid feedback, mixes captured apps, and renders the processed stereo signal to the chosen device. Quitting tears down the taps and routing device so applications resume direct playback.

Closing the window leaves eqMax running in the menu bar. Use the menu bar icon to reopen the window, bypass processing, select a preset, or **Quit eqMax**.

## Keyboard and mouse controls

| Control | Action |
| --- | --- |
| ⌘B | Toggle bypass for EQ and effects |
| ⌘S | Save the current sound as a user preset |
| Arrow keys on a focused knob or band fader | Adjust its value |
| Left/Right on the response graph | Select a band |
| Up/Down on the response graph | Adjust the selected band's gain |
| Shift while adjusting a knob, band fader, or graph point | Use finer steps |
| Double-click a knob, band fader, or graph point | Reset that control |
| Home on a focused knob, band fader, or graph | Reset the control or selected band |

## Tests

CTest runs three suites:

| Suite | Coverage |
| --- | --- |
| `DSPUnitTest` | Filters, EQ response at 44.1/48/96 kHz, spectrum output, limiter transparency and ceiling, and effects bypass |
| `PresetRoundTrip` | Preset serialization, validation, legacy import, saving, and deletion |
| `UIStateAndControls` | Response graph rendering and editing, preset selection, mode changes, bypass, mixer settings persistence, and session restore |

The UI suite runs offscreen without starting audio capture. It writes `build/tests/ui-preview.png` and `build/tests/eq-graph-preview.png` for visual inspection. These automated tests do not verify live system audio capture or physical output devices.

For a short startup check that exits automatically without starting audio capture, run this inside the development shell:

```bash
QT_QPA_PLATFORM=offscreen ./build/eqMax.app/Contents/MacOS/eqMax --smoke-test
```

## Source layout

```text
src/
├── audio/     Core Audio routing, process discovery, and per-app gain state
├── dsp/       Biquad EQ, effects, limiter, and spectrum analysis
├── preset/    Factory presets and user preset storage
├── ui/        Main window, response graph, controls, meters, and menu bar icon
├── docs/      Audio architecture notes
└── main.cpp   Application startup and smoke-test mode
tests/         DSP, preset, and offscreen UI regression suites
macOS/         Bundle metadata, app icon, and icon generator
```
