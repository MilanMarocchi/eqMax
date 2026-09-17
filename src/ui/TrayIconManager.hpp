#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <memory>
#include "../audio/AudioDeviceManager.hpp"
#include "../preset/PresetManager.hpp"

namespace eqmax::ui {

class MainWindow;

class TrayIconManager : public QObject {
    Q_OBJECT
public:
    TrayIconManager(
        MainWindow* mainWindow,
        std::shared_ptr<audio::AudioDeviceManager> audioManager,
        std::shared_ptr<preset::PresetManager> presetManager,
        QObject* parent = nullptr
    );
    ~TrayIconManager() override = default;

    void updatePresetsMenu();

private:
    void createTrayIcon();

    MainWindow* mainWindow_{nullptr};
    std::shared_ptr<audio::AudioDeviceManager> audioManager_;
    std::shared_ptr<preset::PresetManager> presetManager_;

    QSystemTrayIcon* trayIcon_{nullptr};
    QMenu* trayMenu_{nullptr};
    QMenu* presetsMenu_{nullptr};
    QAction* bypassAction_{nullptr};
};

} // namespace eqmax::ui
