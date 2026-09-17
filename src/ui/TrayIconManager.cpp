#include "TrayIconManager.hpp"
#include "MainWindow.hpp"
#include <QPixmap>
#include <QPainter>
#include <QCoreApplication>

namespace eqmax::ui {

TrayIconManager::TrayIconManager(
    MainWindow* mainWindow,
    std::shared_ptr<audio::AudioDeviceManager> audioManager,
    std::shared_ptr<preset::PresetManager> presetManager,
    QObject* parent)
    : QObject(parent),
      mainWindow_(mainWindow),
      audioManager_(std::move(audioManager)),
      presetManager_(std::move(presetManager)) {

    createTrayIcon();
}

void TrayIconManager::createTrayIcon() {
    // Generate a sleek macOS menu bar icon dynamically
    QPixmap iconPixmap(22, 22);
    iconPixmap.fill(Qt::transparent);
    QPainter painter(&iconPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw stylized EQ bars in icon
    painter.setBrush(QColor(0, 229, 255));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(3, 10, 3, 9, 1, 1);
    painter.drawRoundedRect(8, 4, 3, 15, 1, 1);
    painter.drawRoundedRect(13, 8, 3, 11, 1, 1);
    painter.drawRoundedRect(18, 12, 3, 7, 1, 1);

    trayMenu_ = new QMenu();

    // Show/Hide Main Window Action
    auto* showAction = trayMenu_->addAction("Open eqMax Control Center");
    connect(showAction, &QAction::triggered, [this]() {
        mainWindow_->show();
        mainWindow_->raise();
        mainWindow_->activateWindow();
    });

    trayMenu_->addSeparator();

    // Bypass Action
    bypassAction_ = trayMenu_->addAction("Bypass EQ");
    bypassAction_->setCheckable(true);
    connect(bypassAction_, &QAction::triggered, [this](bool checked) {
        mainWindow_->setBypass(checked);
    });

    // Quick Presets Submenu
    presetsMenu_ = trayMenu_->addMenu("Presets");
    updatePresetsMenu();

    trayMenu_->addSeparator();

    // Quit Action
    auto* quitAction = trayMenu_->addAction("Quit eqMax");
    connect(quitAction, &QAction::triggered, []() {
        QCoreApplication::quit();
    });

    trayIcon_ = new QSystemTrayIcon(QIcon(iconPixmap), this);
    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->setToolTip("eqMax - macOS Audio Equalizer & Pro Enhancer");
    trayIcon_->show();

    connect(trayIcon_, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (mainWindow_->isVisible()) {
                mainWindow_->hide();
            } else {
                mainWindow_->show();
                mainWindow_->raise();
                mainWindow_->activateWindow();
            }
        }
    });
}

void TrayIconManager::updatePresetsMenu() {
    if (!presetsMenu_ || !presetManager_) return;
    presetsMenu_->clear();

    auto names = presetManager_->getPresetNames();
    for (const auto& name : names) {
        auto* act = presetsMenu_->addAction(QString::fromStdString(name));
        connect(act, &QAction::triggered, [this, name]() {
            mainWindow_->loadPresetByName(name);
        });
    }
}

} // namespace eqmax::ui
