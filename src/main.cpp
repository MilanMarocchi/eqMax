#include <QApplication>
#include <memory>
#include <iostream>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

#include "EqualizerEngine.hpp"
#include "ProEffectsEngine.hpp"
#include "SpectrumAnalyzer.hpp"
#include "AudioDeviceManager.hpp"
#include "AppVolumeManager.hpp"
#include "PresetManager.hpp"
#include "MainWindow.hpp"
#include "TrayIconManager.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("eqMax Pro");
    app.setOrganizationName("eqMax");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false); // Keep running in menu bar tray when window closed

    // Packaging smoke check: exercise startup without touching preferences or capturing audio.
    const bool smokeTest = app.arguments().contains("--smoke-test");
    QTemporaryDir smokeSettings;
    if (smokeTest) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, smokeSettings.path());
    }

    std::cout << "Starting eqMax Pro (C++20, Qt 6, macOS CoreAudio & vDSP)..." << std::endl;

    // Instantiate core audio DSP engine modules
    auto eqEngine = std::make_shared<eqmax::dsp::EqualizerEngine>();
    auto effectsEngine = std::make_shared<eqmax::dsp::ProEffectsEngine>();
    auto spectrumAnalyzer = std::make_shared<eqmax::dsp::SpectrumAnalyzer>(1024);
    auto appVolumeManager = std::make_shared<eqmax::audio::AppVolumeManager>();
    auto audioManager = smokeTest ? nullptr : std::make_shared<eqmax::audio::AudioDeviceManager>(eqEngine, effectsEngine, spectrumAnalyzer, appVolumeManager);
    auto presetManager = std::make_shared<eqmax::preset::PresetManager>();

    // Instantiate GUI Main Window & System Tray Menu Bar Manager
    eqmax::ui::MainWindow mainWindow(eqEngine, effectsEngine, spectrumAnalyzer, audioManager, appVolumeManager, presetManager);
    eqmax::ui::TrayIconManager trayManager(&mainWindow, audioManager, presetManager);

    mainWindow.show();
    if (smokeTest) QTimer::singleShot(300, &app, &QCoreApplication::quit);
    return app.exec();
}
