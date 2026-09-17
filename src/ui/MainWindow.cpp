#include "MainWindow.hpp"
#include "BasicEQWidget.hpp"
#include <QInputDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QStyle>
#include <iostream>

namespace eqmax::ui {

MainWindow::MainWindow(
    std::shared_ptr<dsp::EqualizerEngine> eqEngine,
    std::shared_ptr<dsp::ProEffectsEngine> effectsEngine,
    std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer,
    std::shared_ptr<audio::AudioDeviceManager> audioManager,
    std::shared_ptr<audio::AppVolumeManager> appVolumeManager,
    std::shared_ptr<preset::PresetManager> presetManager,
    QWidget* parent)
    : QMainWindow(parent),
      eqEngine_(std::move(eqEngine)),
      effectsEngine_(std::move(effectsEngine)),
      spectrumAnalyzer_(std::move(spectrumAnalyzer)),
      audioManager_(std::move(audioManager)),
      appVolumeManager_(std::move(appVolumeManager)),
      presetManager_(std::move(presetManager)) {

    setupUI();
    updateDeviceDropdowns();
    loadPresetByName("Flat");

    // Start Audio Engine
    if (audioManager_) {
        audioManager_->start();
    }
}

void MainWindow::setupUI() {
    setWindowTitle("eqMax Pro - macOS System Equalizer & Enhancer");
    resize(860, 580);
    setMinimumSize(780, 520);

    // Global Dark Slate Theme
    setStyleSheet(R"(
        QMainWindow {
            background-color: #141418;
        }
        QWidget {
            color: #E0E0E0;
            font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "Helvetica Neue", sans-serif;
        }
        QTabWidget::pane {
            border: 1px solid #282832;
            background: #1A1A22;
            border-radius: 8px;
        }
        QTabBar::tab {
            background: #20202A;
            color: #9090A8;
            padding: 10px 20px;
            font-size: 13px;
            font-weight: 600;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 4px;
        }
        QTabBar::tab:selected {
            background: #1A1A22;
            color: #00E5FF;
            border-bottom: 2px solid #00E5FF;
        }
        QComboBox {
            background: #242430;
            border: 1px solid #363646;
            border-radius: 6px;
            padding: 5px 10px;
            color: #FFFFFF;
        }
        QComboBox::drop-down {
            border: none;
        }
        QPushButton {
            background: #282836;
            border: 1px solid #3A3A4C;
            border-radius: 6px;
            padding: 6px 14px;
            color: #FFFFFF;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #323244;
            border-color: #00E5FF;
        }
        QPushButton:pressed {
            background: #00E5FF;
            color: #000000;
        }
        QGroupBox {
            border: 1px solid #2C2C3A;
            border-radius: 8px;
            margin-top: 12px;
            font-weight: bold;
            color: #00E5FF;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
    )");

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    setupHeaderBar(mainLayout);

    // Tab Widget (eqMac signature layout order)
    auto* tabWidget = new QTabWidget(this);
    tabWidget->addTab(createBasicEQTab(), "BASIC");
    tabWidget->addTab(createAdvancedEQTab(), "ADVANCED");
    tabWidget->addTab(createProEffectsTab(), "EXPERT EFFECTS");
    tabWidget->addTab(createAppMixerTab(), "APP MIXER");
    tabWidget->addTab(createSpectrumTab(), "SPECTRUM");
    tabWidget->addTab(createDevicesTab(), "DEVICES");

    mainLayout->addWidget(tabWidget, 1);
    setCentralWidget(central);
}

void MainWindow::setupHeaderBar(QVBoxLayout* mainLayout) {
    auto* header = new QWidget(this);
    header->setStyleSheet("background: #1C1C26; border-radius: 8px; padding: 6px;");
    auto* hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(12, 8, 12, 8);

    // Logo & Title
    auto* titleLabel = new QLabel("eqMax <span style='color: #00E5FF; font-size: 11px;'>PRO</span>", header);
    titleLabel->setTextFormat(Qt::RichText);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: 800; color: #FFFFFF;");
    hLayout->addWidget(titleLabel);

    hLayout->addSpacing(16);

    // Master Power Toggle
    powerBtn_ = new QPushButton("POWER: ON", header);
    powerBtn_->setCheckable(true);
    powerBtn_->setChecked(true);
    powerBtn_->setStyleSheet("QPushButton { background: #00C853; color: #000; border-radius: 14px; font-weight: 800; padding: 6px 14px; }"
                             "QPushButton:checked { background: #00E5FF; color: #000; }"
                             "QPushButton:!checked { background: #E53935; color: #FFF; }");
    connect(powerBtn_, &QPushButton::toggled, this, [this](bool active) {
        setBypass(!active);
        powerBtn_->setText(active ? "POWER: ON" : "BYPASSED");
    });
    hLayout->addWidget(powerBtn_);

    hLayout->addSpacing(20);

    // Output Device Dropdown
    auto* outLabel = new QLabel("Output:", header);
    outLabel->setStyleSheet("color: #A0A0B0; font-size: 12px; font-weight: 600;");
    hLayout->addWidget(outLabel);

    outputDeviceCombo_ = new QComboBox(header);
    outputDeviceCombo_->setMinimumWidth(180);
    connect(outputDeviceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && audioManager_) {
            uint32_t devId = outputDeviceCombo_->itemData(index).toUInt();
            audioManager_->setOutputDevice(devId);
        }
    });
    hLayout->addWidget(outputDeviceCombo_);

    hLayout->addStretch(1);

    // Master Volume Control Slider (-60.0 dB to +24.0 dB)
    auto* volLabel = new QLabel("Volume:", header);
    volLabel->setStyleSheet("color: #A0A0B0; font-size: 12px; font-weight: 600;");
    hLayout->addWidget(volLabel);

    masterVolumeSlider_ = new QSlider(Qt::Horizontal, header);
    masterVolumeSlider_->setRange(-600, 240); // -60.0 dB to +24.0 dB
    masterVolumeSlider_->setValue(0); // 0.0 dB default (100%)
    masterVolumeSlider_->setFixedWidth(140);
    masterVolumeSlider_->setStyleSheet(R"(
        QSlider::groove:horizontal { background: #2A2A3A; height: 6px; border-radius: 3px; }
        QSlider::sub-page:horizontal { background: #00E5FF; border-radius: 3px; }
        QSlider::handle:horizontal { background: #FFF; width: 14px; margin: -4px 0; border-radius: 7px; }
    )");
    hLayout->addWidget(masterVolumeSlider_);

    masterVolumeLabel_ = new QLabel("0.0 dB", header);
    masterVolumeLabel_->setStyleSheet("color: #00E5FF; font-size: 12px; font-weight: bold; width: 60px;");
    hLayout->addWidget(masterVolumeLabel_);

    connect(masterVolumeSlider_, &QSlider::valueChanged, this, [this](int val) {
        float db = static_cast<float>(val) / 10.0f;
        if (val > 0) {
            masterVolumeLabel_->setText(QString("+%1 dB").arg(db, 0, 'f', 1));
            masterVolumeLabel_->setStyleSheet("color: #00E5FF; font-size: 12px; font-weight: bold; width: 60px;");
        } else if (val < 0) {
            masterVolumeLabel_->setText(QString("%1 dB").arg(db, 0, 'f', 1));
            masterVolumeLabel_->setStyleSheet("color: #FF5252; font-size: 12px; font-weight: bold; width: 60px;");
        } else {
            masterVolumeLabel_->setText("0.0 dB");
            masterVolumeLabel_->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: bold; width: 60px;");
        }

        if (effectsEngine_) {
            effectsEngine_->setVolumeBoostdB(db);
        }
        if (volumeBoostSlider_ && volumeBoostSlider_->value() != val) {
            volumeBoostSlider_->blockSignals(true);
            volumeBoostSlider_->setValue(val);
            if (volumeBoostLabel_) {
                volumeBoostLabel_->setText(val > 0 ? QString("+%1 dB").arg(db, 0, 'f', 1) : QString("%1 dB").arg(db, 0, 'f', 1));
            }
            volumeBoostSlider_->blockSignals(false);
        }
    });

    mainLayout->addWidget(header);
}

QWidget* MainWindow::createBasicEQTab() {
    return new BasicEQWidget(eqEngine_, this);
}

QWidget* MainWindow::createAdvancedEQTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(16, 16, 16, 16);

    // Top Controls Bar (EQ Mode, Presets, Save, Flat)
    auto* ctrlBar = new QHBoxLayout();

    // Mode Selector (3-Band / 10-Band / 32-Band)
    auto* modeLabel = new QLabel("Mode:", widget);
    modeLabel->setStyleSheet("font-weight: bold; color: #A0A0B0;");
    ctrlBar->addWidget(modeLabel);

    modeCombo_ = new QComboBox(widget);
    modeCombo_->addItem("3-Band Equalizer", static_cast<int>(dsp::EQMode::ThreeBand));
    modeCombo_->addItem("10-Band Equalizer", static_cast<int>(dsp::EQMode::TenBand));
    modeCombo_->addItem("32-Band Equalizer (Pro)", static_cast<int>(dsp::EQMode::ThirtyTwoBand));
    modeCombo_->setCurrentIndex(1); // Default 10-band

    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        auto mode = static_cast<dsp::EQMode>(modeCombo_->itemData(index).toInt());
        rebuildEQSliders(mode);
    });
    ctrlBar->addWidget(modeCombo_);

    ctrlBar->addSpacing(20);

    // Preset Selector
    auto* presetLabel = new QLabel("Preset:", widget);
    presetLabel->setStyleSheet("font-weight: bold; color: #A0A0B0;");
    ctrlBar->addWidget(presetLabel);

    presetCombo_ = new QComboBox(widget);
    presetCombo_->setMinimumWidth(160);
    if (presetManager_) {
        auto names = presetManager_->getPresetNames();
        for (const auto& n : names) {
            presetCombo_->addItem(QString::fromStdString(n));
        }
    }
    connect(presetCombo_, &QComboBox::textActivated, this, [this](const QString& name) {
        loadPresetByName(name.toStdString());
    });
    ctrlBar->addWidget(presetCombo_);

    // Save Preset Button
    auto* saveBtn = new QPushButton("Save Preset", widget);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveCurrentAsUserPreset);
    ctrlBar->addWidget(saveBtn);

    // Reset Flat Button
    auto* flatBtn = new QPushButton("Flat Reset", widget);
    connect(flatBtn, &QPushButton::clicked, this, [this]() {
        loadPresetByName("Flat");
    });
    ctrlBar->addWidget(flatBtn);

    ctrlBar->addStretch(1);
    layout->addLayout(ctrlBar);

    // Scrollable EQ Sliders View
    auto* scrollArea = new QScrollArea(widget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    sliderContainer_ = new QWidget(scrollArea);
    sliderLayout_ = new QHBoxLayout(sliderContainer_);
    sliderLayout_->setContentsMargins(8, 8, 8, 8);
    sliderLayout_->setSpacing(6);

    scrollArea->setWidget(sliderContainer_);
    layout->addWidget(scrollArea, 1);

    // Real-Time Mini Spectrum Banner underneath sliders
    if (spectrumAnalyzer_) {
        spectrumWidget_ = new SpectrumWidget(spectrumAnalyzer_, widget);
        spectrumWidget_->setMaximumHeight(90);
        layout->addWidget(spectrumWidget_);
    }

    rebuildEQSliders(dsp::EQMode::TenBand);
    return widget;
}

void MainWindow::rebuildEQSliders(dsp::EQMode mode) {
    if (!eqEngine_ || !sliderLayout_) return;

    // Clear existing sliders
    for (auto* slider : eqSliders_) {
        sliderLayout_->removeWidget(slider);
        slider->deleteLater();
    }
    eqSliders_.clear();

    eqEngine_->setMode(mode);
    const auto& infos = eqEngine_->bandInfos();

    for (size_t i = 0; i < infos.size(); ++i) {
        QString labelText;
        float freq = infos[i].frequency;
        if (freq >= 1000.0f) {
            labelText = QString("%1k").arg(freq / 1000.0f, 0, 'f', (freq >= 10000.0f || std::fmod(freq, 1000.0f) == 0) ? 0 : 1);
        } else {
            labelText = QString("%1").arg(static_cast<int>(freq));
        }

        auto* slider = new CustomSlider(labelText, infos[i].gaindB, sliderContainer_);
        connect(slider, &CustomSlider::gainChanged, this, [this, i](float gaindB) {
            if (eqEngine_) {
                eqEngine_->setBandGain(i, gaindB);
            }
        });
        sliderLayout_->addWidget(slider);
        eqSliders_.push_back(slider);
    }
}

QWidget* MainWindow::createProEffectsTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(30);
    grid->setVerticalSpacing(20);

    // 1. Volume Control & Overdrive (-60.0 dB to +24.0 dB)
    auto* groupBoost = new QGroupBox("Master Volume Control (-60 dB to +24 dB)", widget);
    auto* boostLayout = new QVBoxLayout(groupBoost);
    volumeBoostSlider_ = new QSlider(Qt::Horizontal, groupBoost);
    volumeBoostSlider_->setRange(-600, 240);
    volumeBoostSlider_->setValue(0);
    volumeBoostLabel_ = new QLabel("0.0 dB (100%)", groupBoost);
    volumeBoostLabel_->setStyleSheet("color: #00E5FF; font-weight: bold;");
    boostLayout->addWidget(volumeBoostSlider_);
    boostLayout->addWidget(volumeBoostLabel_);
    connect(volumeBoostSlider_, &QSlider::valueChanged, this, [this](int val) {
        float db = static_cast<float>(val) / 10.0f;
        if (val > 0) volumeBoostLabel_->setText(QString("+%1 dB Boost").arg(db, 0, 'f', 1));
        else if (val < 0) volumeBoostLabel_->setText(QString("%1 dB Attenuated").arg(db, 0, 'f', 1));
        else volumeBoostLabel_->setText("0.0 dB (100% Unity)");

        if (effectsEngine_) effectsEngine_->setVolumeBoostdB(db);
        if (masterVolumeSlider_ && masterVolumeSlider_->value() != val) {
            masterVolumeSlider_->blockSignals(true);
            masterVolumeSlider_->setValue(val);
            if (masterVolumeLabel_) {
                masterVolumeLabel_->setText(val > 0 ? QString("+%1 dB").arg(db, 0, 'f', 1) : QString("%1 dB").arg(db, 0, 'f', 1));
            }
            masterVolumeSlider_->blockSignals(false);
        }
    });
    grid->addWidget(groupBoost, 0, 0);

    // 2. Limiter & Soft Clipper Safeguard
    auto* groupLimiter = new QGroupBox("Peak Limiter & Soft Clipper", widget);
    auto* limiterLayout = new QVBoxLayout(groupLimiter);
    limiterToggleBtn_ = new QPushButton("Limiter: ENABLED (Clipping Protection)", groupLimiter);
    limiterToggleBtn_->setCheckable(true);
    limiterToggleBtn_->setChecked(true);
    limiterToggleBtn_->setStyleSheet("QPushButton:checked { background: #00E5FF; color: #000; }"
                                     "QPushButton:!checked { background: #555; color: #FFF; }");
    connect(limiterToggleBtn_, &QPushButton::toggled, this, [this](bool enabled) {
        if (effectsEngine_) effectsEngine_->setLimiterEnabled(enabled);
        limiterToggleBtn_->setText(enabled ? "Limiter: ENABLED (Clipping Protection)" : "Limiter: DISABLED (Raw)");
    });
    limiterLayout->addWidget(limiterToggleBtn_);
    grid->addWidget(groupLimiter, 0, 1);

    // 3. Stereo Balance Control
    auto* groupBalance = new QGroupBox("Stereo Balance", widget);
    auto* balLayout = new QVBoxLayout(groupBalance);
    balanceSlider_ = new QSlider(Qt::Horizontal, groupBalance);
    balanceSlider_->setRange(-100, 100);
    balanceSlider_->setValue(0);
    auto* balLabel = new QLabel("Center", groupBalance);
    balLayout->addWidget(balanceSlider_);
    balLayout->addWidget(balLabel);
    connect(balanceSlider_, &QSlider::valueChanged, this, [this, balLabel](int val) {
        float bal = static_cast<float>(val) / 100.0f;
        if (effectsEngine_) effectsEngine_->setBalance(bal);
        if (val < 0) balLabel->setText(QString("Left %1%").arg(-val));
        else if (val > 0) balLabel->setText(QString("Right %1%").arg(val));
        else balLabel->setText("Center");
    });
    grid->addWidget(groupBalance, 1, 0);

    // 4. 3D Spatial Audio Widener
    auto* groupSpatial = new QGroupBox("3D Spatial Surround Sound", widget);
    auto* spatialLayout = new QVBoxLayout(groupSpatial);
    spatialSlider_ = new QSlider(Qt::Horizontal, groupSpatial);
    spatialSlider_->setRange(0, 200); // 0.0 to 2.0
    spatialSlider_->setValue(100); // 1.0 = Normal stereo
    auto* spatialLabel = new QLabel("100% Stereo Width", groupSpatial);
    spatialLayout->addWidget(spatialSlider_);
    spatialLayout->addWidget(spatialLabel);
    connect(spatialSlider_, &QSlider::valueChanged, this, [this, spatialLabel](int val) {
        float w = static_cast<float>(val) / 100.0f;
        if (effectsEngine_) effectsEngine_->setSpatialWidth(w);
        spatialLabel->setText(QString("%1% Spatial Width").arg(val));
    });
    grid->addWidget(groupSpatial, 1, 1);

    // 5. Deep Bass Booster
    auto* groupBass = new QGroupBox("Sub-Bass Booster", widget);
    auto* bassLayout = new QVBoxLayout(groupBass);
    bassBoostSlider_ = new QSlider(Qt::Horizontal, groupBass);
    bassBoostSlider_->setRange(0, 120);
    bassBoostSlider_->setValue(0);
    auto* bassLabel = new QLabel("+0.0 dB Bass Boost", groupBass);
    bassLayout->addWidget(bassBoostSlider_);
    bassLayout->addWidget(bassLabel);
    connect(bassBoostSlider_, &QSlider::valueChanged, this, [this, bassLabel](int val) {
        float db = static_cast<float>(val) / 10.0f;
        if (effectsEngine_) effectsEngine_->setBassBoostdB(db);
        bassLabel->setText(QString("+%1 dB Bass Boost").arg(db, 0, 'f', 1));
    });
    grid->addWidget(groupBass, 2, 0);

    // 6. Air / Treble Enhancer
    auto* groupTreble = new QGroupBox("Treble & Clarity Enhancer", widget);
    auto* trebleLayout = new QVBoxLayout(groupTreble);
    trebleBoostSlider_ = new QSlider(Qt::Horizontal, groupTreble);
    trebleBoostSlider_->setRange(0, 120);
    trebleBoostSlider_->setValue(0);
    auto* trebleLabel = new QLabel("+0.0 dB Treble Boost", groupTreble);
    trebleLayout->addWidget(trebleBoostSlider_);
    trebleLayout->addWidget(trebleLabel);
    connect(trebleBoostSlider_, &QSlider::valueChanged, this, [this, trebleLabel](int val) {
        float db = static_cast<float>(val) / 10.0f;
        if (effectsEngine_) effectsEngine_->setTrebleBoostdB(db);
        trebleLabel->setText(QString("+%1 dB Treble Boost").arg(db, 0, 'f', 1));
    });
    grid->addWidget(groupTreble, 2, 1);

    layout->addLayout(grid);
    layout->addStretch(1);
    return widget;
}

QWidget* MainWindow::createSpectrumTab() {
    auto* widget = new QWidget(this);
    auto* outerLayout = new QVBoxLayout(widget);
    outerLayout->setContentsMargins(16, 16, 16, 16);
    outerLayout->setSpacing(12);

    // Top Control Banner for Permissions & Test Audio
    auto* bannerLayout = new QHBoxLayout();
    
    auto* permBtn = new QPushButton("Request macOS Audio Permission", widget);
    permBtn->setStyleSheet("QPushButton { background: #00E5FF; color: #000; font-weight: bold; border-radius: 6px; padding: 6px 12px; }"
                           "QPushButton:hover { background: #00B0FF; }");
    connect(permBtn, &QPushButton::clicked, this, [this]() {
        if (audioManager_) {
            audioManager_->requestAudioPermissions();
            QMessageBox::information(this, "macOS Permission Request", 
                "macOS Audio Capture Permission has been requested.\n\n"
                "If prompted, please click 'Allow' in the macOS System Dialog.\n"
                "You can also verify in: System Settings -> Privacy & Security -> Microphone.");
        }
    });
    bannerLayout->addWidget(permBtn);

    auto* demoBtn = new QPushButton("Toggle Spectrum Demo (Pink Noise Generator)", widget);
    demoBtn->setCheckable(true);
    demoBtn->setStyleSheet("QPushButton { background: #28283A; color: #FFF; font-weight: bold; border-radius: 6px; padding: 6px 12px; }"
                           "QPushButton:checked { background: #00C853; color: #000; }");
    connect(demoBtn, &QPushButton::toggled, this, [this, demoBtn](bool active) {
        if (audioManager_) {
            audioManager_->setTestSignalType(active ? audio::TestSignalType::PinkNoise : audio::TestSignalType::None);
            demoBtn->setText(active ? "Spectrum Demo: PLAYING (Pink Noise)" : "Toggle Spectrum Demo (Pink Noise Generator)");
        }
    });
    bannerLayout->addWidget(demoBtn);

    bannerLayout->addStretch(1);
    outerLayout->addLayout(bannerLayout);

    auto* spectrumContent = new QWidget(widget);
    auto* layout = new QHBoxLayout(spectrumContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    if (spectrumAnalyzer_) {
        auto* largeSpectrum = new SpectrumWidget(spectrumAnalyzer_, spectrumContent);
        largeSpectrum->setNumBands(64);
        layout->addWidget(largeSpectrum, 3);

        vuMeterWidget_ = new VUMeterWidget(spectrumAnalyzer_, spectrumContent);
        layout->addWidget(vuMeterWidget_, 1);
    }

    outerLayout->addWidget(spectrumContent, 1);
    return widget;
}

QWidget* MainWindow::createDevicesTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(16);

    // Audio Output Selector
    auto* outCombo = new QComboBox(widget);
    outCombo->addItem("System Speakers", 1);
    outCombo->addItem("Headphones", 2);
    formLayout->addRow("Output Device:", outCombo);

    // Test Signal Generator Selector
    testSignalCombo_ = new QComboBox(widget);
    testSignalCombo_->addItem("Off (Normal Audio Processing)", static_cast<int>(audio::TestSignalType::None));
    testSignalCombo_->addItem("1 kHz Sine Wave Tone", static_cast<int>(audio::TestSignalType::Sine1kHz));
    testSignalCombo_->addItem("Pink Noise Audio Generator", static_cast<int>(audio::TestSignalType::PinkNoise));
    testSignalCombo_->addItem("Sine Frequency Sweep (40Hz - 15kHz)", static_cast<int>(audio::TestSignalType::SineSweep));
    connect(testSignalCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        auto type = static_cast<audio::TestSignalType>(testSignalCombo_->itemData(index).toInt());
        if (audioManager_) audioManager_->setTestSignalType(type);
    });
    formLayout->addRow("Audio Test Signal Generator:", testSignalCombo_);

    // Info readout
    auto* infoLabel = new QLabel("Sample Rate: 44.1 kHz | Buffer Size: 512 frames | Latency: 11.6 ms", widget);
    infoLabel->setStyleSheet("color: #00E5FF; font-weight: bold; font-size: 13px;");
    formLayout->addRow("Diagnostics:", infoLabel);

    layout->addLayout(formLayout);
    layout->addStretch(1);
    return widget;
}

void MainWindow::updateDeviceDropdowns() {
    if (!audioManager_ || !outputDeviceCombo_) return;
    outputDeviceCombo_->clear();

    auto devices = audioManager_->getAudioDevices();
    for (const auto& dev : devices) {
        if (dev.isOutput) {
            outputDeviceCombo_->addItem(QString::fromStdString(dev.name), dev.id);
        }
    }
}

void MainWindow::loadPresetByName(const std::string& name) {
    if (!presetManager_ || !eqEngine_) return;
    auto pOpt = presetManager_->getPreset(name);
    if (!pOpt.has_value()) return;

    const auto& p = pOpt.value();
    eqEngine_->setAllGains(p.gains);

    // Update UI Sliders
    for (size_t i = 0; i < eqSliders_.size() && i < p.gains.size(); ++i) {
        eqSliders_[i]->setGaindB(p.gains[i]);
    }

    if (presetCombo_) {
        presetCombo_->setCurrentText(QString::fromStdString(p.name));
    }

    if (effectsEngine_) {
        effectsEngine_->setVolumeBoostdB(p.volumeBoostdB);
        effectsEngine_->setSpatialWidth(p.spatialWidth);
        effectsEngine_->setBassBoostdB(p.bassBoostdB);
        effectsEngine_->setTrebleBoostdB(p.trebleBoostdB);
    }
}

void MainWindow::saveCurrentAsUserPreset() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Save User Preset", "Preset Name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    preset::Preset p;
    p.name = name.trimmed().toStdString();
    p.mode = eqEngine_->mode();

    for (auto* slider : eqSliders_) {
        p.gains.push_back(slider->gaindB());
    }

    if (effectsEngine_) {
        p.volumeBoostdB = effectsEngine_->volumeBoostdB();
        p.spatialWidth = effectsEngine_->spatialWidth();
        p.bassBoostdB = effectsEngine_->bassBoostdB();
        p.trebleBoostdB = effectsEngine_->trebleBoostdB();
    }

    if (presetManager_->saveUserPreset(p)) {
        presetCombo_->addItem(name);
        presetCombo_->setCurrentText(name);
        QMessageBox::information(this, "Success", "Preset saved successfully!");
    }
}

QWidget* MainWindow::createAppMixerTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(16, 16, 16, 16);

    // Top control bar for App Mixer
    auto* topBar = new QHBoxLayout();
    auto* titleLabel = new QLabel("Per-Application Audio Volume Mixer", widget);
    titleLabel->setStyleSheet("font-size: 15px; font-weight: bold; color: #00E5FF;");
    topBar->addWidget(titleLabel);

    topBar->addStretch(1);

    auto* refreshBtn = new QPushButton("Refresh Applications", widget);
    topBar->addWidget(refreshBtn);
    layout->addLayout(topBar);

    // Scrollable container for app volume rows
    auto* scrollArea = new QScrollArea(widget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* listContainer = new QWidget(scrollArea);
    auto* containerLayout = new QVBoxLayout(listContainer);
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(10);

    scrollArea->setWidget(listContainer);
    layout->addWidget(scrollArea, 1);

    auto refreshAction = [this, containerLayout]() {
        refreshAppMixerList(containerLayout);
    };

    connect(refreshBtn, &QPushButton::clicked, this, refreshAction);
    refreshAction(); // Initial scan

    return widget;
}

void MainWindow::refreshAppMixerList(QVBoxLayout* containerLayout) {
    if (!containerLayout || !appVolumeManager_) return;

    // Clear previous items
    QLayoutItem* child;
    while ((child = containerLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    auto apps = appVolumeManager_->getRunningApplications();

    if (apps.empty()) {
        auto* emptyLabel = new QLabel("No active GUI applications detected.", containerLayout->parentWidget());
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #707080; font-size: 14px; margin-top: 40px;");
        containerLayout->addWidget(emptyLabel);
        return;
    }

    for (const auto& app : apps) {
        auto* card = new QWidget(containerLayout->parentWidget());
        card->setStyleSheet("background: #1C1C26; border-radius: 8px; padding: 10px;");
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);

        // App Info
        auto* nameLabel = new QLabel(QString::fromStdString(app.appName), card);
        nameLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #FFFFFF;");
        nameLabel->setFixedWidth(180);
        cardLayout->addWidget(nameLabel);

        auto* pidBadge = new QLabel(QString("PID: %1").arg(app.pid), card);
        pidBadge->setStyleSheet("color: #707085; font-size: 11px;");
        pidBadge->setFixedWidth(70);
        cardLayout->addWidget(pidBadge);

        // Volume Slider (0% to 200%)
        auto* slider = new QSlider(Qt::Horizontal, card);
        slider->setRange(0, 200);
        slider->setValue(static_cast<int>(app.volume * 100.0f));
        slider->setStyleSheet(R"(
            QSlider::groove:horizontal { background: #2A2A3A; height: 6px; border-radius: 3px; }
            QSlider::sub-page:horizontal { background: #00E5FF; border-radius: 3px; }
            QSlider::handle:horizontal { background: #FFF; width: 14px; margin: -4px 0; border-radius: 7px; }
        )");
        cardLayout->addWidget(slider, 1);

        // Volume Readout
        auto* volReadout = new QLabel(QString("%1%").arg(static_cast<int>(app.volume * 100.0f)), card);
        volReadout->setStyleSheet("color: #00E5FF; font-weight: bold; font-size: 12px; width: 45px;");
        cardLayout->addWidget(volReadout);

        // Mute Button
        int pid = app.pid;
        auto* muteBtn = new QPushButton(app.isMuted ? "MUTED" : "Mute", card);
        muteBtn->setCheckable(true);
        muteBtn->setChecked(app.isMuted);
        muteBtn->setStyleSheet(app.isMuted ? "background: #E53935; color: #FFF; font-weight: bold;"
                                           : "background: #282836; color: #FFF;");
        
        connect(slider, &QSlider::valueChanged, this, [this, pid, volReadout](int val) {
            float vol = static_cast<float>(val) / 100.0f;
            volReadout->setText(QString("%1%").arg(val));
            if (appVolumeManager_) appVolumeManager_->setAppVolume(pid, vol);
        });

        connect(muteBtn, &QPushButton::toggled, this, [this, pid, muteBtn](bool checked) {
            muteBtn->setText(checked ? "MUTED" : "Mute");
            muteBtn->setStyleSheet(checked ? "background: #E53935; color: #FFF; font-weight: bold;"
                                           : "background: #282836; color: #FFF;");
            if (appVolumeManager_) appVolumeManager_->setAppMute(pid, checked);
        });

        cardLayout->addWidget(muteBtn);
        containerLayout->addWidget(card);
    }
    containerLayout->addStretch(1);
}

void MainWindow::setBypass(bool bypassed) {
    if (eqEngine_) {
        eqEngine_->setBypass(bypassed);
    }
}

} // namespace eqmax::ui

