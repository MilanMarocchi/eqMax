# eqMax Pro - macOS System Audio Equalizer & Pro Enhancer

**eqMax** is a high-performance system-wide audio equalizer and pro sound enhancer for macOS, built with **Modern C++20**, **Qt 6**, **Apple CoreAudio**, and Apple **vDSP (Accelerate framework)**. It takes inspiration from eqMac’s compact audio controls, with a native Qt interface and Core Audio process-tap routing on macOS 14.2 or newer.

---

## 🌟 Features & Pro Capabilities

- 🎚️ **Multi-Band Equalizer Engine**:
  - **3-Band Mode**: Quick Bass, Mid, and Treble shelf adjustment.
  - **10-Band Mode**: ISO standard 10-band octave frequencies (32 Hz - 16 kHz).
  - **32-Band Mode (Pro)**: High-precision 1/3 octave ISO standard band EQ (20 Hz - 20 kHz).
  - Stereo IIR biquad cascades, with coefficient changes applied on the audio thread at buffer boundaries and tuning matched to the output sample rate.
- 🔊 **Volume Boost Overdrive (+24 dB)**:
  - Amplifies audio volume beyond 100% standard limits.
  - Stereo-linked **peak limiter** with a 0.98 ceiling and 50 ms release. Signals below the ceiling pass through without saturation at unity settings.
- 🎧 **3D Spatial Audio & Surround Sound**:
  - Mid-side stereo width adjustment, from mono through normal stereo to wider stereo.
- ⚖️ **Stereo Balance & Channel Control**:
  - Logarithmic constant-power panning law.
- 🎸 **Sub-Bass & Treble Clarity Sub-Engine**:
  - Dedicated instant-boost dials for deep punchy bass and crystalline high frequencies.
- 📊 **60 FPS Real-Time Spectrum Analyzer & VU Meter**:
  - Hardware-accelerated Qt custom canvas rendering vDSP FFT frequency spectrum bars with peak hold decay and L/R channel VU level meters with overload LEDs.
- 🎛️ **Preset Management System**:
  - Includes 11 Factory Presets (*Flat, Bass Booster, Treble Booster, Rock, Pop, Jazz, Electronic, Vocal, Classical, Acoustic, Gaming/Movies*).
  - Create, save, delete, export, and import custom user presets stored in `~/.config/eqMax/presets/`. New files use validated JSON and atomic writes; the original line-based files remain readable.
- 📌 **macOS System Tray Menu Bar Integration**:
  - Quick access from the macOS status bar (`QSystemTrayIcon`): bypass EQ and effects, switch presets on the fly, or show/hide the main window.
- 🛠️ **Audio Routing & Built-in Signal Generator**:
  - CoreAudio device selector (Speakers, Headphones, Display Audio), refreshed when devices connect or disconnect. Internal routing devices are hidden.
  - Built-in audio test signal generator (1 kHz Sine, Pink Noise, Frequency Sweep) for out-of-the-box testing without requiring kernel drivers.

---

## 🚀 Quick Start with Nix

Everything in the project is managed with **Nix** (`flake.nix`).

### 1. Enter Development Shell
```bash
nix develop
```

### 2. Build the Application
```bash
nix develop --command bash -c "mkdir -p build && cd build && cmake .. -GNinja && ninja"
```

### 3. Run DSP Unit Tests
```bash
nix develop --command ctest --test-dir build --output-on-failure
```

### 4. Launch eqMax App
```bash
nix develop --command ./build/eqMax.app/Contents/MacOS/eqMax
```

---

## Everyday controls

- Drag the points on the EQ response graph, use the band faders, or switch to Basic for Bass / Mid / Treble knobs. Expert offers a **fixed 32-band graphic EQ**, rather than a parametric EQ.
- Arrow keys adjust focused knobs and faders. On the graph, Left/Right selects a band and Up/Down changes its gain. Shift gives finer steps; Home resets the selected control.
- **⌘B** bypasses EQ and effects; application mixer levels remain independent. **⌘S** saves a preset.
- The preset menu (**•••**) imports, exports, and deletes presets. Imported sounds open for auditioning; use Save to add them to your library. Editing a sound changes the selector to Custom.
- Sound settings, application levels/mutes, selected output, window geometry, and collapsed sections restore on the next launch. Test signals always start off.
- Output → Setup provides test signals, the system audio permission link, and a routing restart button. macOS prompts for system audio recording access when capture starts.

The regression suite checks DSP response at 44.1/48/96 kHz, limiter transparency and bypass, preset validation and legacy loading, graph editing, mixer persistence, and session restore. The UI test runs offscreen without starting audio capture and writes `build/tests/ui-preview.png` for visual inspection.

## 📂 Project Architecture

```
eqMax/
├── flake.nix                  # Nix Flake environment & build specification
├── CMakeLists.txt             # CMake build configuration (C++20, Qt6, CoreAudio, vDSP)
├── src/
│   ├── main.cpp               # App entry point (QApplication, TrayIcon, MainWindow)
│   ├── dsp/                   # High-performance DSP Engine
│   │   ├── BiquadFilter.hpp/.cpp      # 2nd-order IIR RBJ filter cascades
│   │   ├── EqualizerEngine.hpp/.cpp   # 3 / 10 / 32 band EQ engine
│   │   ├── ProEffectsEngine.hpp/.cpp  # Volume boost, Limiter, 3D Spatial, Balance
│   │   └── SpectrumAnalyzer.hpp/.cpp  # vDSP FFT & logarithmic band energy binning
│   ├── audio/                 # macOS CoreAudio & Driver Interface
│   │   └── AudioDeviceManager.hpp/.mm # CoreAudio HAL enumeration & IOProc callback
│   ├── preset/                # Preset Storage System
│   │   └── PresetManager.hpp/.cpp     # Factory & JSON user preset management
│   └── ui/                    # Modern macOS Dark Qt6 GUI
│       ├── CustomSlider.hpp/.cpp      # Vertical EQ gain slider widget
│       ├── SpectrumWidget.hpp/.cpp    # 60 FPS spectrum analyzer canvas
│       ├── VUMeterWidget.hpp/.cpp      # Stereo L/R level meters & overload LEDs
│       ├── MainWindow.hpp/.cpp        # Control Center main window & tabs
│       └── TrayIconManager.hpp/.cpp   # System menu bar tray integration
└── tests/
    ├── CMakeLists.txt         # Unit tests build script
    └── test_dsp.cpp           # DSP engine test suite
```

---

## 🛠️ Tech Stack & Standards

- **Language**: C++20 (`std::span`, `std::numbers`, RAII, atomic lock-free queues, smart pointers)
- **GUI Framework**: Qt 6.11 (QtWidgets, custom QPainter 60FPS visualizers)
- **Audio DSP**: Apple Accelerate vDSP & CoreAudio HAL framework
- **Package & Dev Environment**: Nix Flakes
- **Build System**: CMake 3.20+ & Ninja
