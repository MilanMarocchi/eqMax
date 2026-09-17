#include "BasicEQWidget.hpp"
#include <QGroupBox>

namespace eqmax::ui {

BasicEQWidget::BasicEQWidget(std::shared_ptr<dsp::EqualizerEngine> eqEngine, QWidget* parent)
    : QWidget(parent), eqEngine_(std::move(eqEngine)) {

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    // Title / Description Banner
    auto* banner = new QLabel("BASIC EQUALIZER - 3 BAND QUICK CONTROLS", this);
    banner->setStyleSheet("font-size: 13px; font-weight: bold; color: #00E5FF; letter-spacing: 1px;");
    layout->addWidget(banner);

    // 3 Main Equalizer Cards (BASS, MID, TREBLE)
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(20);

    cardsLayout->addWidget(createBandCard("BASS", "100 Hz Low Shelf", 0));
    cardsLayout->addWidget(createBandCard("MID", "1 kHz Peaking Filter", 1));
    cardsLayout->addWidget(createBandCard("TREBLE", "10 kHz High Shelf", 2));

    layout->addLayout(cardsLayout);

    // Quick Action Bar (Flat Reset, Bass Boost, Vocal Boost, Treble Boost)
    auto* quickBar = new QHBoxLayout();
    quickBar->setSpacing(12);

    auto createQuickBtn = [this](const QString& text, float b, float m, float t) {
        auto* btn = new QPushButton(text, this);
        btn->setStyleSheet("QPushButton { background: #262636; border: 1px solid #3A3A50; border-radius: 6px; padding: 8px 16px; font-weight: bold; color: #E0E0E0; }"
                           "QPushButton:hover { background: #323246; border-color: #00E5FF; color: #00E5FF; }");
        connect(btn, &QPushButton::clicked, this, [this, b, m, t]() {
            if (eqEngine_) {
                eqEngine_->setMode(dsp::EQMode::ThreeBand);
                eqEngine_->setBandGain(0, b);
                eqEngine_->setBandGain(1, m);
                eqEngine_->setBandGain(2, t);
                updateValues();
            }
        });
        return btn;
    };

    quickBar->addWidget(createQuickBtn("Flat Reset", 0.0f, 0.0f, 0.0f));
    quickBar->addWidget(createQuickBtn("Punchy Bass", 6.0f, 1.0f, 0.0f));
    quickBar->addWidget(createQuickBtn("Vocal Boost", -1.0f, 4.0f, 2.0f));
    quickBar->addWidget(createQuickBtn("Crisp Treble", 0.0f, 1.0f, 5.5f));

    layout->addLayout(quickBar);
    layout->addStretch(1);

    updateValues();
}

QWidget* BasicEQWidget::createBandCard(const QString& title, const QString& freqText, size_t bandIdx) {
    auto* card = new QWidget(this);
    card->setStyleSheet("background: #1C1C26; border-radius: 12px; padding: 16px; border: 1px solid #282836;");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // Header (BASS / MID / TREBLE)
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: 800; color: #FFFFFF;");
    layout->addWidget(titleLabel);

    auto* freqLabel = new QLabel(freqText, card);
    freqLabel->setAlignment(Qt::AlignCenter);
    freqLabel->setStyleSheet("font-size: 11px; color: #808096;");
    layout->addWidget(freqLabel);

    // dB Gain Readout
    auto* valueLabel = new QLabel("0.0 dB", card);
    valueLabel->setAlignment(Qt::AlignCenter);
    valueLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00E5FF; margin: 4px 0;");
    layout->addWidget(valueLabel);

    // Slider (-12dB to +12dB)
    auto* slider = new QSlider(Qt::Vertical, card);
    slider->setRange(-120, 120);
    slider->setValue(0);
    slider->setMinimumHeight(150);
    slider->setStyleSheet(R"(
        QSlider::groove:vertical { background: #2A2A3A; width: 8px; border-radius: 4px; }
        QSlider::sub-page:vertical { background: #2A2A3A; border-radius: 4px; }
        QSlider::add-page:vertical { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #00E5FF, stop:1 #0088FF); border-radius: 4px; }
        QSlider::handle:vertical { background: #FFFFFF; border: 2px solid #00E5FF; height: 20px; margin-left: -6px; margin-right: -6px; border-radius: 10px; }
        QSlider::handle:vertical:hover { background: #00E5FF; border: 2px solid #FFFFFF; }
    )");
    layout->addWidget(slider, 1, Qt::AlignHCenter);

    // Reset Button
    auto* resetBtn = new QPushButton("0 dB", card);
    resetBtn->setStyleSheet("QPushButton { background: #262634; border: 1px solid #36364A; border-radius: 4px; color: #A0A0B0; font-size: 11px; padding: 4px 8px; }"
                            "QPushButton:hover { background: #323246; color: #00E5FF; }");
    connect(resetBtn, &QPushButton::clicked, this, [slider]() {
        slider->setValue(0);
    });
    layout->addWidget(resetBtn, 0, Qt::AlignHCenter);

    connect(slider, &QSlider::valueChanged, this, [this, bandIdx, valueLabel](int val) {
        float gaindB = static_cast<float>(val) / 10.0f;
        if (gaindB > 0.05f) {
            valueLabel->setText(QString("+%1 dB").arg(gaindB, 0, 'f', 1));
            valueLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00E5FF;");
        } else if (gaindB < -0.05f) {
            valueLabel->setText(QString("%1 dB").arg(gaindB, 0, 'f', 1));
            valueLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FF5252;");
        } else {
            valueLabel->setText("0.0 dB");
            valueLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #A0A0B0;");
        }

        if (eqEngine_) {
            if (eqEngine_->mode() != dsp::EQMode::ThreeBand) {
                eqEngine_->setMode(dsp::EQMode::ThreeBand);
            }
            eqEngine_->setBandGain(bandIdx, gaindB);
        }
    });

    if (bandIdx == 0) bassSlider_ = slider, bassLabel_ = valueLabel;
    else if (bandIdx == 1) midSlider_ = slider, midLabel_ = valueLabel;
    else if (bandIdx == 2) trebleSlider_ = slider, trebleLabel_ = valueLabel;

    return card;
}

void BasicEQWidget::updateValues() {
    if (!eqEngine_) return;
    if (eqEngine_->mode() != dsp::EQMode::ThreeBand) {
        eqEngine_->setMode(dsp::EQMode::ThreeBand);
    }

    float b = eqEngine_->bandGain(0);
    float m = eqEngine_->bandGain(1);
    float t = eqEngine_->bandGain(2);

    if (bassSlider_) bassSlider_->setValue(static_cast<int>(b * 10.0f));
    if (midSlider_) midSlider_->setValue(static_cast<int>(m * 10.0f));
    if (trebleSlider_) trebleSlider_->setValue(static_cast<int>(t * 10.0f));
}

} // namespace eqmax::ui
