# eqMax Pro - macOS System Audio Equalizer & Pro Enhancer

**eqMax** is a high-performance system-wide audio equalizer and pro sound enhancer for macOS, built with **Modern C++20**, **Qt 6**, **Apple CoreAudio**, and Apple **vDSP (Accelerate framework)**. It is an open-source clone of eqMac, featuring all their Pro capabilities.

---

## 🌟 Features & Pro Capabilities

- 🎚️ **Multi-Band Equalizer Engine**:
  - **3-Band Mode**: Quick Bass, Mid, and Treble shelf adjustment.
  - **10-Band Mode**: ISO standard 10-band octave frequencies (32 Hz - 16 kHz).
  - **32-Band Mode (Pro)**: High-precision 1/3 octave ISO standard band EQ (20 Hz - 20 kHz).
  - Dynamic smooth coefficient interpolation using 2nd-order IIR Biquad filter cascades.
- 🔊 **Volume Boost Overdrive (+24 dB)**:
  - Amplifies audio volume beyond 100% standard limits.
  - Integrated **Soft-Clipper & Peak Limiter** safeguard (tanh analog saturation) to prevent digital clipping & distortion.
- 🎧 **3D Spatial Audio & Surround Sound**:
  - Mid-Side matrix stereo decomposition & Haas effect spatial width widening.
- ⚖️ **Stereo Balance & Channel Control**:
  - Logarithmic constant-power panning law.
- 🎸 **Sub-Bass & Treble Clarity Sub-Engine**:
  - Dedicated instant-boost dials for deep punchy bass and crystalline high frequencies.
- 📊 **60 FPS Real-Time Spectrum Analyzer & VU Meter**:
  - Hardware-accelerated Qt custom canvas rendering vDSP FFT frequency spectrum bars with peak hold decay and L/R channel VU level meters with overload LEDs.
- 🎛️ **Preset Management System**:
  - Includes 11 Factory Presets (*Flat, Bass Booster, Treble Booster, Rock, Pop, Jazz, Electronic, Vocal, Classical, Acoustic, Gaming/Movies*).
  - Create, save, export, and import custom user presets stored in `~/.config/eqMax/presets/`.
- 📌 **macOS System Tray Menu Bar Integration**:
  - Quick access from the macOS status bar (`QSystemTrayIcon`): toggle bypass, switch presets on the fly, adjust volume, or show/hide the main window.
- 🛠️ **Audio Routing & Built-in Signal Generator**:
  - CoreAudio device selector (Speakers, Headphones, Display Audio).
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
nix develop --command ./build/tests/test_dsp
```

### 4. Launch eqMax App
```bash
nix develop --command ./build/eqMax.app/Contents/MacOS/eqMax
```

---

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
