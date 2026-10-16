#include "MainWindow.hpp"
#include "Theme.hpp"
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QFileDialog>
#include <QMenu>
#include <QSettings>
#include <QSignalBlocker>
#include <QShortcut>
#include <QGuiApplication>
#include <cmath>
#include <set>

namespace eqmax::ui {

namespace {

// Small speaker glyph drawn at runtime (0-2 sound waves)
QPixmap speakerIcon(int waves, qreal dpr) {
    const int size = 16;
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(theme::TextSecondary);
    QPainterPath body;
    body.moveTo(1.5, 6);
    body.lineTo(4.5, 6);
    body.lineTo(8, 2.5);
    body.lineTo(8, 13.5);
    body.lineTo(4.5, 10);
    body.lineTo(1.5, 10);
    body.closeSubpath();
    p.drawPath(body);
    p.setPen(QPen(theme::TextSecondary, 1.4, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < waves; ++i) {
        const qreal r = 3.0 + i * 3.0;
        p.drawArc(QRectF(8 - r, 8 - r, 2 * r, 2 * r), -50 * 16, 100 * 16);
    }
    return pixmap;
}

QPixmap logoIcon(qreal dpr) {
    const int size = 20;
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(theme::Accent);
    const std::array<int, 4> heights = { 8, 16, 11, 6 };
    for (size_t i = 0; i < heights.size(); ++i) {
        p.drawRoundedRect(QRectF(2 + i * 4.5, 18 - heights[i], 3, heights[i]), 1.5, 1.5);
    }
    return pixmap;
}

QLabel* secondaryLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QString("color: %1;").arg(theme::css(theme::TextSecondary)));
    return label;
}

QString formatVolume(float db) {
    if (std::abs(db) < 0.05f) return "0 dB";
    return QString("%1%2 dB").arg(db > 0 ? "+" : "").arg(db, 0, 'f', 1);
}

QString formatFrequency(float freq) {
    if (freq >= 1000.0f) {
        return QString("%1k").arg(freq / 1000.0f, 0, 'f', std::fmod(freq, 1000.0f) == 0.0f ? 0 : 1);
    }
    return QString::number(freq, 'g', 3);
}

} // namespace

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

    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(350);
    connect(&saveTimer_, &QTimer::timeout, this, &MainWindow::saveSession);
    setupUI();
    updateDeviceDropdowns();
    restoreSession();
    auto* bypassShortcut = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(bypassShortcut, &QShortcut::activated, this, [this]() { setBypass(!isBypassed()); });
    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveCurrentAsUserPreset);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSession);

    // Start Audio Engine
    if (audioManager_) {
        audioManager_->start();
    }

    connect(&statusTimer_, &QTimer::timeout, this, [this]() {
        updateDeviceDropdowns();
        updateStatus();
        refreshAppMixerList();
    });
    statusTimer_.start(2000);
    updateStatus();
}

void MainWindow::setupUI() {
    setWindowTitle("eqMax");
    resize(480, 900);
    setMinimumSize(460, 520);
    setStyleSheet(theme::globalStyleSheet());

    auto* central = new QWidget(this);
    central->setObjectName("central");
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    mainLayout->addWidget(createHeader());

    auto* scrollArea = new QScrollArea(central);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* column = new QWidget(scrollArea);
    auto* columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(12, 4, 12, 12);
    columnLayout->setSpacing(10);

    // Keep the destination visible before the sound controls, as in a menu bar audio utility.
    cards_ = {createOutputCard(), createVolumeCard(), createEqualizerCard(),
              createEffectsCard(), createApplicationsCard(), createAnalyzerCard()};
    const std::array<const char*, 6> keys = {"output", "volume", "equalizer", "effects", "applications", "analyzer"};
    for (size_t i = 0; i < cards_.size(); ++i) {
        cards_[i]->setObjectName(keys[i]);
        // Keep a stable key for the section's saved collapsed state.
        cards_[i]->setStyleSheet(QString("QFrame#%1 { background: %2; border: 1px solid %3; border-radius: 10px; }")
            .arg(keys[i], theme::css(theme::Card), theme::css(theme::CardBorder)));
        connect(cards_[i], &SectionCard::collapsedChanged, this, [this]() { scheduleSessionSave(); });
        columnLayout->addWidget(cards_[i]);
    }
    columnLayout->addStretch(1);

    scrollArea->setWidget(column);
    mainLayout->addWidget(scrollArea, 1);
    setCentralWidget(central);
}

QWidget* MainWindow::createHeader() {
    auto* header = new QWidget(this);
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(16, 12, 16, 8);
    layout->setSpacing(8);

    auto* logo = new QLabel(header);
    logo->setPixmap(logoIcon(devicePixelRatioF()));
    layout->addWidget(logo);

    auto* title = new QLabel("eqMax", header);
    title->setStyleSheet(QString("font-size: 17px; font-weight: 700; color: %1;").arg(theme::css(theme::TextPrimary)));
    layout->addWidget(title);

    layout->addStretch(1);

    statusLabel_ = new QLabel(header);
    statusLabel_->setStyleSheet(QString("color: %1; font-size: 11px;").arg(theme::css(theme::TextSecondary)));
    layout->addWidget(statusLabel_);

    layout->addSpacing(6);

    powerBtn_ = new PowerButton(header);
    powerBtn_->setChecked(true);
    powerBtn_->setAccessibleName("Enable audio processing");
    powerBtn_->setToolTip("Enable or bypass EQ and effects (⌘B)");
    connect(powerBtn_, &PowerButton::toggled, this, [this](bool active) {
        setBypass(!active);
    });
    layout->addWidget(powerBtn_);

    return header;
}

SectionCard* MainWindow::createVolumeCard() {
    auto* card = new SectionCard("Volume", this);
    const qreal dpr = devicePixelRatioF();

    // Master volume (-60 dB .. +24 dB boost)
    auto* volumeRow = new QHBoxLayout();
    volumeRow->setSpacing(8);
    auto* quietIcon = new QLabel(card);
    quietIcon->setPixmap(speakerIcon(0, dpr));
    volumeRow->addWidget(quietIcon);

    masterVolumeSlider_ = new QSlider(Qt::Horizontal, card);
    masterVolumeSlider_->setObjectName("masterVolume");
    masterVolumeSlider_->setAccessibleName("Output gain in decibels");
    masterVolumeSlider_->setRange(-600, 240);
    masterVolumeSlider_->setValue(0);
    masterVolumeSlider_->setToolTip("Output gain: -60 dB to +24 dB (boost above 0 dB uses the limiter)");
    volumeRow->addWidget(masterVolumeSlider_, 1);

    auto* loudIcon = new QLabel(card);
    loudIcon->setPixmap(speakerIcon(2, dpr));
    volumeRow->addWidget(loudIcon);

    masterVolumeLabel_ = new QLabel("0 dB", card);
    masterVolumeLabel_->setFixedWidth(58);
    masterVolumeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    volumeRow->addWidget(masterVolumeLabel_);
    card->contentLayout()->addLayout(volumeRow);

    connect(masterVolumeSlider_, &QSlider::valueChanged, this, [this](int val) {
        const float db = static_cast<float>(val) / 10.0f;
        masterVolumeLabel_->setText(formatVolume(db));
        masterVolumeLabel_->setStyleSheet(val > 0 ? QString("color: %1;").arg(theme::css(theme::Accent)) : QString());
        if (effectsEngine_) effectsEngine_->setVolumeBoostdB(db);
        markPresetModified();
    });
    connect(masterVolumeSlider_, &QSlider::sliderReleased, this, [this]() {
        if (std::abs(masterVolumeSlider_->value()) <= 10) masterVolumeSlider_->setValue(0);
    });

    // Balance
    auto* balanceRow = new QHBoxLayout();
    balanceRow->setSpacing(8);
    auto* leftLabel = secondaryLabel("L", card);
    leftLabel->setFixedWidth(16);
    leftLabel->setAlignment(Qt::AlignCenter);
    balanceRow->addWidget(leftLabel);

    balanceSlider_ = new QSlider(Qt::Horizontal, card);
    balanceSlider_->setObjectName("balance");
    balanceSlider_->setAccessibleName("Stereo balance");
    balanceSlider_->setProperty("bipolar", true);
    balanceSlider_->setRange(-100, 100);
    balanceSlider_->setValue(0);
    balanceSlider_->setToolTip("Stereo balance");
    balanceRow->addWidget(balanceSlider_, 1);

    auto* rightLabel = secondaryLabel("R", card);
    rightLabel->setFixedWidth(16);
    rightLabel->setAlignment(Qt::AlignCenter);
    balanceRow->addWidget(rightLabel);

    balanceLabel_ = new QLabel("Center", card);
    balanceLabel_->setFixedWidth(58);
    balanceLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    balanceRow->addWidget(balanceLabel_);
    card->contentLayout()->addLayout(balanceRow);

    connect(balanceSlider_, &QSlider::valueChanged, this, [this](int val) {
        if (effectsEngine_) effectsEngine_->setBalance(static_cast<float>(val) / 100.0f);
        if (val < 0) balanceLabel_->setText(QString("L %1%").arg(-val));
        else if (val > 0) balanceLabel_->setText(QString("R %1%").arg(val));
        else balanceLabel_->setText("Center");
        markPresetModified();
    });
    connect(balanceSlider_, &QSlider::sliderReleased, this, [this]() {
        // Snap back to center when released close to it
        if (std::abs(balanceSlider_->value()) <= 4) balanceSlider_->setValue(0);
    });

    return card;
}

SectionCard* MainWindow::createEqualizerCard() {
    auto* card = new SectionCard("Equalizers", this);

    // Basic / Advanced / Expert segmented control
    auto* segment = new QWidget(card);
    segment->setObjectName("segment");
    segment->setStyleSheet(QString("QWidget#segment { background: %1; border-radius: 6px; }").arg(theme::css(theme::Control)));
    auto* segmentLayout = new QHBoxLayout(segment);
    segmentLayout->setContentsMargins(2, 2, 2, 2);
    segmentLayout->setSpacing(2);
    const std::array<std::pair<QString, dsp::EQMode>, 3> types = {{
        { "Basic", dsp::EQMode::ThreeBand },
        { "Advanced", dsp::EQMode::TenBand },
        { "Expert", dsp::EQMode::ThirtyTwoBand },
    }};
    for (size_t i = 0; i < types.size(); ++i) {
        auto* button = new QPushButton(types[i].first, segment);
        button->setProperty("segment", true);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        const dsp::EQMode mode = types[i].second;
        connect(button, &QPushButton::clicked, this, [this, mode]() {
            if (eqEngine_ && eqEngine_->mode() == mode) return;
            setEqualizerType(mode);
            markPresetModified();
        });
        segmentLayout->addWidget(button);
        eqTypeButtons_[i] = button;
    }
    card->addHeaderWidget(segment);

    // Presets
    auto* presetRow = new QHBoxLayout();
    presetRow->setSpacing(6);
    presetCombo_ = new QComboBox(card);
    presetCombo_->setObjectName("presetSelector");
    presetCombo_->setAccessibleName("Equalizer preset");
    refreshPresetCombo("Flat");
    connect(presetCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        const auto name = presetCombo_->itemData(index).toString();
        if (!name.isEmpty()) loadPresetByName(name.toStdString());
    });
    presetRow->addWidget(presetCombo_, 1);

    auto* saveBtn = new QPushButton("Save", card);
    saveBtn->setToolTip("Save the current settings as a preset");
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveCurrentAsUserPreset);
    presetRow->addWidget(saveBtn);

    auto* moreBtn = new QPushButton("•••", card);
    moreBtn->setAccessibleName("Manage presets");
    moreBtn->setFixedWidth(34);
    moreBtn->setStyleSheet("QPushButton { padding: 4px; }");
    auto* presetMenu = new QMenu(moreBtn);
    presetMenu->addAction("Import preset…", this, &MainWindow::importPreset);
    presetMenu->addAction("Export current settings…", this, &MainWindow::exportPreset);
    auto* deleteAction = presetMenu->addAction("Delete user preset", this, &MainWindow::deleteCurrentPreset);
    connect(presetMenu, &QMenu::aboutToShow, this, [this, deleteAction]() {
        const auto p = presetManager_ ? presetManager_->getPreset(selectedPreset_.toStdString()) : std::nullopt;
        deleteAction->setEnabled(p && !p->isFactory);
    });
    moreBtn->setMenu(presetMenu);
    presetRow->addWidget(moreBtn);

    auto* flatBtn = new QPushButton("Reset", card);
    flatBtn->setToolTip("Flatten all bands");
    connect(flatBtn, &QPushButton::clicked, this, [this]() {
        if (!eqEngine_) return;
        eqEngine_->resetGains();
        for (auto* slider : eqSliders_) slider->setGaindB(0.0f);
        if (basicEQ_) basicEQ_->updateValues();
        markPresetModified();
    });
    presetRow->addWidget(flatBtn);
    card->contentLayout()->addLayout(presetRow);

    response_ = new EQResponseWidget(eqEngine_, card);
    connect(response_, &EQResponseWidget::bandEdited, this, [this](size_t index, float gain) {
        if (!eqEngine_) return;
        eqEngine_->setBandGain(index, gain);
        if (index < eqSliders_.size()) eqSliders_[index]->setGaindB(gain);
        if (basicEQ_) basicEQ_->updateValues();
        markPresetModified();
    });
    card->contentLayout()->addWidget(response_);

    // Basic knobs / band sliders
    eqStack_ = new QStackedWidget(card);
    basicEQ_ = new BasicEQWidget(eqEngine_, eqStack_);
    eqStack_->addWidget(basicEQ_);
    connect(basicEQ_, &BasicEQWidget::gainsChanged, this, &MainWindow::markPresetModified);

    auto* sliderScroll = new QScrollArea(eqStack_);
    sliderScroll->setWidgetResizable(true);
    sliderScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sliderScroll->setMinimumHeight(180);
    sliderContainer_ = new QWidget(sliderScroll);
    sliderLayout_ = new QHBoxLayout(sliderContainer_);
    sliderLayout_->setContentsMargins(0, 0, 0, 0);
    sliderLayout_->setSpacing(0);
    sliderScroll->setWidget(sliderContainer_);
    eqStack_->addWidget(sliderScroll);
    card->contentLayout()->addWidget(eqStack_);

    setEqualizerType(eqEngine_ ? eqEngine_->mode() : dsp::EQMode::TenBand, false);
    return card;
}

SectionCard* MainWindow::createEffectsCard() {
    auto* card = new SectionCard("Effects", this);

    auto* knobRow = new QHBoxLayout();
    knobRow->setSpacing(0);

    const auto dbFormatter = [](double v) {
        return std::abs(v) < 0.05 ? QString("Off") : QString("+%1 dB").arg(v, 0, 'f', 1);
    };

    bassBoostKnob_ = new KnobWidget("Bass Boost", 0.0, 12.0, 0.0, card);
    bassBoostKnob_->setValueFormatter(dbFormatter);
    connect(bassBoostKnob_, &KnobWidget::valueChanged, this, [this](double v) {
        if (effectsEngine_) effectsEngine_->setBassBoostdB(static_cast<float>(v));
        markPresetModified();
    });

    trebleBoostKnob_ = new KnobWidget("Clarity", 0.0, 12.0, 0.0, card);
    trebleBoostKnob_->setValueFormatter(dbFormatter);
    connect(trebleBoostKnob_, &KnobWidget::valueChanged, this, [this](double v) {
        if (effectsEngine_) effectsEngine_->setTrebleBoostdB(static_cast<float>(v));
        markPresetModified();
    });

    spatialKnob_ = new KnobWidget("Stereo Width", 0.0, 200.0, 100.0, card);
    spatialKnob_->setValueFormatter([](double v) { return QString("%1%").arg(static_cast<int>(std::round(v))); });
    connect(spatialKnob_, &KnobWidget::valueChanged, this, [this](double v) {
        if (effectsEngine_) effectsEngine_->setSpatialWidth(static_cast<float>(v / 100.0));
        markPresetModified();
    });

    for (auto* knob : { bassBoostKnob_, trebleBoostKnob_, spatialKnob_ }) {
        knobRow->addStretch(1);
        knobRow->addWidget(knob);
    }
    knobRow->addStretch(1);
    card->contentLayout()->addLayout(knobRow);

    auto* limiterRow = new QHBoxLayout();
    limiterRow->addWidget(new QLabel("Limiter", card));
    auto* limiterHint = new QLabel("prevents clipping when boosting", card);
    limiterHint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(theme::css(theme::TextTertiary)));
    limiterRow->addWidget(limiterHint);
    limiterRow->addStretch(1);

    limiterToggleBtn_ = new QPushButton("On", card);
    limiterToggleBtn_->setCheckable(true);
    limiterToggleBtn_->setChecked(true);
    limiterToggleBtn_->setFixedWidth(54);
    limiterToggleBtn_->setStyleSheet(QString("QPushButton:checked { background: %1; border-color: %1; color: #FFFFFF; font-weight: 600; }")
                                         .arg(theme::css(theme::Accent)));
    connect(limiterToggleBtn_, &QPushButton::toggled, this, [this](bool enabled) {
        if (effectsEngine_) effectsEngine_->setLimiterEnabled(enabled);
        limiterToggleBtn_->setText(enabled ? "On" : "Off");
        markPresetModified();
    });
    limiterRow->addWidget(limiterToggleBtn_);
    card->contentLayout()->addLayout(limiterRow);

    return card;
}

SectionCard* MainWindow::createApplicationsCard() {
    auto* card = new SectionCard("Applications", this);

    auto* refreshBtn = new QPushButton("Refresh", card);
    refreshBtn->setProperty("flat", true);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshAppMixerList);
    card->addHeaderWidget(refreshBtn);

    appEmptyLabel_ = secondaryLabel("No applications using audio.", card);
    appEmptyLabel_->setAlignment(Qt::AlignCenter);
    card->contentLayout()->addWidget(appEmptyLabel_);

    auto* list = new QWidget(card);
    appListLayout_ = new QVBoxLayout(list);
    appListLayout_->setContentsMargins(0, 0, 0, 0);
    appListLayout_->setSpacing(4);
    card->contentLayout()->addWidget(list);

    refreshAppMixerList();
    return card;
}

QWidget* MainWindow::createAppRow(const audio::AppAudioInfo& app) {
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(8);

    auto* dot = new QLabel(row);
    dot->setFixedSize(8, 8);
    layout->addWidget(dot);

    auto* nameLabel = new QLabel(row);
    nameLabel->setFixedWidth(118);
    nameLabel->setToolTip(QString("%1\nPID %2").arg(QString::fromStdString(app.appKey)).arg(app.pid));
    nameLabel->setText(nameLabel->fontMetrics().elidedText(QString::fromStdString(app.appName), Qt::ElideRight, nameLabel->width()));
    layout->addWidget(nameLabel);

    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, 200);
    slider->setValue(static_cast<int>(std::round(app.volume * 100.0f)));
    slider->setToolTip("Application volume (up to 200%)");
    layout->addWidget(slider, 1);

    auto* readout = new QLabel(QString("%1%").arg(slider->value()), row);
    readout->setFixedWidth(40);
    readout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(readout);

    auto* muteBtn = new QPushButton(row);
    muteBtn->setCheckable(true);
    muteBtn->setChecked(app.isMuted);
    muteBtn->setFixedWidth(30);
    muteBtn->setIcon(QIcon(speakerIcon(app.isMuted ? 0 : 1, devicePixelRatioF())));
    muteBtn->setToolTip("Mute");
    muteBtn->setStyleSheet(QString("QPushButton { padding: 2px; } QPushButton:checked { background: %1; border-color: %1; }")
                               .arg(theme::css(theme::Danger)));
    layout->addWidget(muteBtn);

    const std::string appKey = app.appKey;
    connect(slider, &QSlider::valueChanged, this, [this, appKey, readout](int val) {
        readout->setText(QString("%1%").arg(val));
        if (appVolumeManager_) appVolumeManager_->setAppVolume(appKey, static_cast<float>(val) / 100.0f);
        scheduleSessionSave();
    });
    connect(slider, &QSlider::sliderReleased, this, [slider]() {
        // Snap to 100% when released close to it
        if (std::abs(slider->value() - 100) <= 3) slider->setValue(100);
    });
    connect(muteBtn, &QPushButton::toggled, this, [this, appKey, muteBtn](bool muted) {
        muteBtn->setIcon(QIcon(speakerIcon(muted ? 0 : 1, devicePixelRatioF())));
        if (appVolumeManager_) appVolumeManager_->setAppMute(appKey, muted);
        scheduleSessionSave();
    });

    appRows_[appKey] = { row, dot };
    return row;
}

void MainWindow::refreshAppMixerList() {
    if (!appListLayout_ || !appVolumeManager_) return;

    const auto apps = appVolumeManager_->getRunningApplications();

    // Remove rows for apps that went away
    std::set<std::string> present;
    for (const auto& app : apps) present.insert(app.appKey);
    for (auto it = appRows_.begin(); it != appRows_.end();) {
        if (!present.count(it->first)) {
            appListLayout_->removeWidget(it->second.widget);
            it->second.widget->deleteLater();
            it = appRows_.erase(it);
        } else {
            ++it;
        }
    }

    // Add new rows and update "playing" indicators without disturbing existing sliders
    for (const auto& app : apps) {
        auto it = appRows_.find(app.appKey);
        if (it == appRows_.end()) {
            appListLayout_->addWidget(createAppRow(app));
            it = appRows_.find(app.appKey);
        }
        it->second.playingDot->setStyleSheet(QString("background: %1; border-radius: 4px;")
                                                 .arg(theme::css(app.isPlaying ? theme::Success : theme::Track)));
        it->second.playingDot->setToolTip(app.isPlaying ? "Playing audio" : "Idle");
    }

    appEmptyLabel_->setVisible(appRows_.empty());
}

SectionCard* MainWindow::createAnalyzerCard() {
    auto* card = new SectionCard("Analyzer", this);

    if (spectrumAnalyzer_) {
        auto* row = new QHBoxLayout();
        row->setSpacing(10);
        auto* spectrum = new SpectrumWidget(spectrumAnalyzer_, card);
        spectrum->setNumBands(64);
        spectrum->setMinimumHeight(150);
        row->addWidget(spectrum, 1);

        auto* meter = new VUMeterWidget(spectrumAnalyzer_, card);
        meter->setFixedWidth(56);
        row->addWidget(meter);
        card->contentLayout()->addLayout(row);
    }

    card->setCollapsed(true);
    return card;
}

SectionCard* MainWindow::createOutputCard() {
    auto* card = new SectionCard("Output", this);

    outputDeviceCombo_ = new QComboBox(card);
    connect(outputDeviceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && audioManager_) {
            audioManager_->setOutputDevice(outputDeviceCombo_->itemData(index).toUInt());
            updateStatus();
            scheduleSessionSave();
        }
    });
    outputDeviceCombo_->setObjectName("outputDevice");
    outputDeviceCombo_->setAccessibleName("Audio output device");
    card->contentLayout()->addWidget(outputDeviceCombo_);

    auto* setupBtn = new QPushButton("Setup", card);
    setupBtn->setProperty("flat", true);
    setupBtn->setCheckable(true);
    card->addHeaderWidget(setupBtn);
    auto* setup = new QWidget(card);
    auto* setupLayout = new QVBoxLayout(setup);
    setupLayout->setContentsMargins(0, 0, 0, 0);
    setupLayout->setSpacing(8);
    connect(setupBtn, &QPushButton::toggled, setup, &QWidget::setVisible);
    setup->hide();

    auto* testRow = new QHBoxLayout();
    testRow->addWidget(secondaryLabel("Test signal", card));
    testSignalCombo_ = new QComboBox(card);
    testSignalCombo_->addItem("Off", static_cast<int>(audio::TestSignalType::None));
    testSignalCombo_->addItem("1 kHz Sine", static_cast<int>(audio::TestSignalType::Sine1kHz));
    testSignalCombo_->addItem("Pink Noise", static_cast<int>(audio::TestSignalType::PinkNoise));
    testSignalCombo_->addItem("Sine Sweep", static_cast<int>(audio::TestSignalType::SineSweep));
    connect(testSignalCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        auto type = static_cast<audio::TestSignalType>(testSignalCombo_->itemData(index).toInt());
        if (audioManager_) audioManager_->setTestSignalType(type);
    });
    testRow->addWidget(testSignalCombo_, 1);
    setupLayout->addLayout(testRow);

    auto* permissionBtn = new QPushButton("System Audio Recording permission…", card);
    permissionBtn->setProperty("flat", true);
    permissionBtn->setCursor(Qt::PointingHandCursor);
    permissionBtn->setStyleSheet(QString("QPushButton { color: %1; text-align: left; } QPushButton:hover { color: %2; }")
                                     .arg(theme::css(theme::Accent), theme::css(theme::TextPrimary)));
    connect(permissionBtn, &QPushButton::clicked, this, [this]() {
        if (audioManager_) audioManager_->requestAudioPermissions();
        QMessageBox::information(this, "System Audio Recording Permission",
            "eqMax captures system audio with Core Audio process taps.\n\n"
            "Enable eqMax under: System Settings -> Privacy & Security -> "
            "Screen & System Audio Recording -> System Audio Recording Only.\n\n"
            "If audio is silent after enabling it, restart eqMax.");
    });
    setupLayout->addWidget(permissionBtn, 0, Qt::AlignLeft);
    auto* retryBtn = new QPushButton("Restart audio routing", setup);
    connect(retryBtn, &QPushButton::clicked, this, [this]() {
        if (audioManager_) { audioManager_->stop(); audioManager_->start(); }
        updateStatus();
    });
    setupLayout->addWidget(retryBtn);
    card->contentLayout()->addWidget(setup);

    return card;
}

void MainWindow::setEqualizerType(dsp::EQMode mode, bool resetEngine) {
    if (!eqEngine_) return;
    if (resetEngine && eqEngine_->mode() != mode) {
        eqEngine_->setMode(mode);
    }

    const int typeIndex = mode == dsp::EQMode::ThreeBand ? 0 : (mode == dsp::EQMode::TenBand ? 1 : 2);
    for (size_t i = 0; i < eqTypeButtons_.size(); ++i) {
        eqTypeButtons_[i]->setChecked(static_cast<int>(i) == typeIndex);
    }

    // Size the stack to the visible page only (Basic knobs are much shorter than band sliders)
    const int pageIndex = mode == dsp::EQMode::ThreeBand ? 0 : 1;
    for (int i = 0; i < eqStack_->count(); ++i) {
        const auto policy = i == pageIndex ? QSizePolicy::Preferred : QSizePolicy::Ignored;
        eqStack_->widget(i)->setSizePolicy(policy, policy);
    }

    if (mode == dsp::EQMode::ThreeBand) {
        eqStack_->setCurrentIndex(0);
        basicEQ_->updateValues();
    } else {
        eqStack_->setCurrentIndex(1);
        rebuildEQSliders();
    }
    eqStack_->setFixedHeight(mode == dsp::EQMode::ThreeBand ? basicEQ_->sizeHint().height() : 192);
    if (response_) response_->refresh();
}

void MainWindow::rebuildEQSliders() {
    if (!eqEngine_ || !sliderLayout_) return;

    for (auto* slider : eqSliders_) {
        slider->hide();
        slider->deleteLater();
    }
    eqSliders_.clear();
    while (QLayoutItem* item = sliderLayout_->takeAt(0)) delete item;

    const bool compact = eqEngine_->mode() == dsp::EQMode::ThirtyTwoBand;
    const auto& infos = eqEngine_->bandInfos();
    for (size_t i = 0; i < infos.size(); ++i) {
        auto* slider = new CustomSlider(formatFrequency(infos[i].frequency), infos[i].gaindB, sliderContainer_);
        slider->setCompact(compact);
        connect(slider, &CustomSlider::gainChanged, this, [this, i](float gaindB) {
            if (eqEngine_) eqEngine_->setBandGain(i, gaindB);
            markPresetModified();
        });
        sliderLayout_->addWidget(slider, 1);
        eqSliders_.push_back(slider);
    }
}

void MainWindow::updateDeviceDropdowns() {
    if (!outputDeviceCombo_) return;
    const auto selectedUid = outputDeviceCombo_->currentIndex() >= 0
        ? outputDeviceCombo_->currentData(Qt::UserRole + 1).toString() : QString();
    std::vector<audio::AudioDeviceInfo> outputs;
    if (audioManager_) {
        for (const auto& device : audioManager_->getAudioDevices()) if (device.isOutput) outputs.push_back(device);
    }
    bool changed = outputDeviceCombo_->count() != static_cast<int>(outputs.size()) + 1;
    for (size_t i = 0; !changed && i < outputs.size(); ++i) {
        changed = outputDeviceCombo_->itemData(static_cast<int>(i) + 1).toUInt() != outputs[i].id ||
                  outputDeviceCombo_->itemText(static_cast<int>(i) + 1) != QString::fromStdString(outputs[i].name);
    }
    if (!changed) return;
    const QSignalBlocker blocker(outputDeviceCombo_);
    outputDeviceCombo_->clear();
    outputDeviceCombo_->addItem("System Default Output", 0u);
    outputDeviceCombo_->setItemData(0, QString(), Qt::UserRole + 1);
    for (const auto& dev : outputs) {
        outputDeviceCombo_->addItem(QString::fromStdString(dev.name), dev.id);
        outputDeviceCombo_->setItemData(outputDeviceCombo_->count() - 1, QString::fromStdString(dev.uid), Qt::UserRole + 1);
    }
    const int index = outputDeviceCombo_->findData(selectedUid, Qt::UserRole + 1);
    outputDeviceCombo_->setCurrentIndex(std::max(0, index));
    if (index < 0 && audioManager_) audioManager_->setOutputDevice(0);
}

void MainWindow::updateStatus() {
    if (!statusLabel_) return;
    if (isBypassed()) {
        statusLabel_->setText("Bypassed");
        return;
    }
    if (!audioManager_ || !audioManager_->isRoutingActive()) {
        statusLabel_->setText("Not routing");
        return;
    }
    statusLabel_->setText(QString("%1 kHz").arg(audioManager_->currentSampleRate() / 1000.0, 0, 'g', 3));
    if (response_) response_->setSampleRate(audioManager_->currentSampleRate());
}

void MainWindow::loadPresetByName(const std::string& name) {
    if (!presetManager_ || !eqEngine_) return;
    const auto p = presetManager_->getPreset(name);
    if (!p) return;
    applyPreset(*p);
    selectedPreset_ = QString::fromStdString(p->name);
    refreshPresetCombo(selectedPreset_);
    scheduleSessionSave();
}

void MainWindow::applyPreset(const preset::Preset& p) {
    if (!eqEngine_) return;
    const bool wasApplying = applyingState_;
    applyingState_ = true;
    eqEngine_->setMode(p.mode);
    eqEngine_->setAllGains(p.gains);
    setEqualizerType(p.mode, false);
    if (effectsEngine_) {
        effectsEngine_->setSpatialWidth(p.spatialWidth);
        effectsEngine_->setBassBoostdB(p.bassBoostdB);
        effectsEngine_->setTrebleBoostdB(p.trebleBoostdB);
        effectsEngine_->setBalance(p.balance);
        effectsEngine_->setLimiterEnabled(p.limiterEnabled);
    }
    setMasterVolumedB(p.volumeBoostdB);
    balanceSlider_->setValue(static_cast<int>(std::round(p.balance * 100)));
    bassBoostKnob_->setValue(p.bassBoostdB);
    trebleBoostKnob_->setValue(p.trebleBoostdB);
    spatialKnob_->setValue(p.spatialWidth * 100);
    limiterToggleBtn_->setChecked(p.limiterEnabled);
    if (response_) response_->refresh();
    applyingState_ = wasApplying;
}

void MainWindow::setMasterVolumedB(float db) {
    if (masterVolumeSlider_) masterVolumeSlider_->setValue(static_cast<int>(std::round(db * 10)));
    if (effectsEngine_) effectsEngine_->setVolumeBoostdB(db);
}

preset::Preset MainWindow::currentPreset(const std::string& name) const {
    preset::Preset p;
    p.name = name;
    if (eqEngine_) {
        p.mode = eqEngine_->mode();
        for (size_t i = 0; i < eqEngine_->numBands(); ++i) p.gains.push_back(eqEngine_->bandGain(i));
    }
    if (effectsEngine_) {
        p.volumeBoostdB = effectsEngine_->volumeBoostdB();
        p.spatialWidth = effectsEngine_->spatialWidth();
        p.bassBoostdB = effectsEngine_->bassBoostdB();
        p.trebleBoostdB = effectsEngine_->trebleBoostdB();
        p.balance = effectsEngine_->balance();
        p.limiterEnabled = effectsEngine_->isLimiterEnabled();
    }
    return p;
}

void MainWindow::refreshPresetCombo(const QString& selected) {
    if (!presetCombo_) return;
    const QSignalBlocker blocker(presetCombo_);
    presetCombo_->clear();
    presetCombo_->addItem("Custom", QString());
    if (presetManager_) {
        for (const auto& name : presetManager_->getPresetNames()) {
            const auto text = QString::fromStdString(name);
            presetCombo_->addItem(text, text);
        }
    }
    presetCombo_->setCurrentIndex(std::max(0, presetCombo_->findData(selected)));
}

void MainWindow::markPresetModified() {
    if (applyingState_) return;
    selectedPreset_.clear();
    if (presetCombo_) presetCombo_->setCurrentIndex(0);
    if (response_) response_->refresh();
    scheduleSessionSave();
}

void MainWindow::saveCurrentAsUserPreset() {
    if (!eqEngine_ || !presetManager_) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, "Save Preset", "Preset name:", QLineEdit::Normal, selectedPreset_, &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const auto existing = presetManager_->getPreset(name.toStdString());
    if (existing && existing->isFactory) {
        QMessageBox::information(this, "Save Preset", "Choose a new name to keep the factory preset available.");
        return;
    }
    if (existing && QMessageBox::question(this, "Replace Preset", "Replace “" + name + "” with the current settings?") != QMessageBox::Yes) return;
    if (!presetManager_->saveUserPreset(currentPreset(name.toStdString()))) {
        QMessageBox::warning(this, "Save Preset", "Could not save the preset. Check the name and preset folder permissions.");
        return;
    }
    selectedPreset_ = name;
    refreshPresetCombo(name);
    emit presetsChanged();
    scheduleSessionSave();
}

void MainWindow::importPreset() {
    if (!presetManager_) return;
    const auto path = QFileDialog::getOpenFileName(this, "Import eqMax Preset", {}, "eqMax presets (*.eqpreset *.json)");
    if (path.isEmpty()) return;
    const auto p = preset::PresetManager::readPresetFile(path);
    if (!p) {
        QMessageBox::warning(this, "Import Preset", "This file is not a valid eqMax preset. Use an exported .eqpreset file.");
        return;
    }
    applyPreset(*p);
    markPresetModified();
    // Import opens the sound for auditioning; Save lets the user choose a library name.
}

void MainWindow::exportPreset() {
    const auto p = currentPreset(selectedPreset_.isEmpty() ? "Custom" : selectedPreset_.toStdString());
    const auto path = QFileDialog::getSaveFileName(this, "Export eqMax Preset", "Preset.eqpreset", "eqMax presets (*.eqpreset)");
    if (!path.isEmpty() && !preset::PresetManager::writePresetFile(path, p)) {
        QMessageBox::warning(this, "Export Preset", "Could not write the preset file.");
    }
}

void MainWindow::deleteCurrentPreset() {
    if (!presetManager_) return;
    const auto p = presetManager_->getPreset(selectedPreset_.toStdString());
    if (!p || p->isFactory) return;
    if (QMessageBox::question(this, "Delete Preset", "Delete “" + selectedPreset_ + "” from your presets?") != QMessageBox::Yes) return;
    if (!presetManager_->deleteUserPreset(p->name)) {
        QMessageBox::warning(this, "Delete Preset", "Could not delete the preset file.");
        return;
    }
    markPresetModified();
    refreshPresetCombo();
    emit presetsChanged();
}

bool MainWindow::isBypassed() const { return eqEngine_ && eqEngine_->isBypassed(); }

void MainWindow::setBypass(bool bypassed) {
    if (powerBtn_) {
        const QSignalBlocker blocker(powerBtn_);
        powerBtn_->setChecked(!bypassed);
    }
    if (eqEngine_) eqEngine_->setBypass(bypassed);
    if (effectsEngine_) effectsEngine_->setBypass(bypassed);
    if (eqStack_) eqStack_->setEnabled(!bypassed);
    if (response_) response_->setEnabled(!bypassed);
    updateStatus();
    emit bypassChanged(bypassed);
    scheduleSessionSave();
}

MainWindow::~MainWindow() { saveSession(); }

void MainWindow::scheduleSessionSave() {
    if (!applyingState_) saveTimer_.start();
}

void MainWindow::saveSession() {
    if (appVolumeManager_) appVolumeManager_->saveSettings();
    QSettings settings;
    settings.setValue("session/sound", preset::PresetManager::serialize(currentPreset()));
    settings.setValue("session/preset", selectedPreset_);
    settings.setValue("session/bypassed", isBypassed());
    settings.setValue("window/geometry", saveGeometry());
    if (outputDeviceCombo_) settings.setValue("session/outputUID", outputDeviceCombo_->currentData(Qt::UserRole + 1));
    for (auto* card : cards_) settings.setValue("sections/" + card->objectName(), card->isCollapsed());
}

void MainWindow::restoreSession() {
    applyingState_ = true;
    QSettings settings;
    const auto p = preset::PresetManager::deserialize(settings.value("session/sound").toByteArray());
    if (p) {
        applyPreset(*p);
        selectedPreset_ = settings.value("session/preset").toString();
        refreshPresetCombo(selectedPreset_);
    } else {
        loadPresetByName("Flat");
    }
    restoreGeometry(settings.value("window/geometry").toByteArray());
    for (auto* card : cards_) {
        card->setCollapsed(settings.value("sections/" + card->objectName(), card->isCollapsed()).toBool());
    }
    const auto uid = settings.value("session/outputUID").toString();
    const int index = outputDeviceCombo_->findData(uid, Qt::UserRole + 1);
    outputDeviceCombo_->setCurrentIndex(std::max(0, index));
    setBypass(settings.value("session/bypassed", false).toBool());
    applyingState_ = false;
}

} // namespace eqmax::ui
