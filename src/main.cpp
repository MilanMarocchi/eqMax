#include <QApplication>
#include <memory>
#include <iostream>

#include "EqualizerEngine.hpp"
#include "ProEffectsEngine.hpp"
#include "SpectrumAnalyzer.hpp"
#include "AudioDeviceManager.hpp"
#include "PresetManager.hpp"
#include "MainWindow.hpp"
#include "TrayIconManager.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("eqMax Pro");
    app.setOrganizationName("eqMax");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false); // Keep running in menu bar tray when window closed

    std::cout << "Starting eqMax Pro (C++20, Qt 6, macOS CoreAudio & vDSP)..." << std::endl;

    // Instantiate core audio DSP engine modules
    auto eqEngine = std::make_shared<eqmax::dsp::EqualizerEngine>();
    auto effectsEngine = std::make_shared<eqmax::dsp::ProEffectsEngine>();
    auto spectrumAnalyzer = std::make_shared<eqmax::dsp::SpectrumAnalyzer>(1024);
    auto audioManager = std::make_shared<eqmax::audio::AudioDeviceManager>(eqEngine, effectsEngine, spectrumAnalyzer);
    auto presetManager = std::make_shared<eqmax::preset::PresetManager>();

    // Instantiate GUI Main Window & System Tray Menu Bar Manager
    eqmax::ui::MainWindow mainWindow(eqEngine, effectsEngine, spectrumAnalyzer, audioManager, presetManager);
    eqmax::ui::TrayIconManager trayManager(&mainWindow, audioManager, presetManager);

    mainWindow.show();
    return app.exec();
}
