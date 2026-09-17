#include "CustomSlider.hpp"
#include <QFont>
#include <QStyle>

namespace eqmax::ui {

CustomSlider::CustomSlider(const QString& freqText, float initialGaindB, QWidget* parent)
    : QWidget(parent) {

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 4, 2, 4);
    layout->setSpacing(4);

    // Value Label (+0.0 dB)
    valueLabel_ = new QLabel("0.0 dB", this);
    valueLabel_->setAlignment(Qt::AlignCenter);
    valueLabel_->setStyleSheet("color: #00E5FF; font-size: 11px; font-weight: bold;");

    // Slider (-24 dB to +24 dB mapped to -240 .. +240 integer steps)
    slider_ = new QSlider(Qt::Vertical, this);
    slider_->setRange(-120, 120); // -12.0dB to +12.0dB range for fine control
    slider_->setValue(static_cast<int>(initialGaindB * 10.0f));
    slider_->setFocusPolicy(Qt::NoFocus);

    // Modern QSlider CSS styling matching eqMac dark UI
    slider_->setStyleSheet(R"(
        QSlider::groove:vertical {
            background: #2A2A36;
            width: 6px;
            border-radius: 3px;
        }
        QSlider::sub-page:vertical {
            background: #2A2A36;
            border-radius: 3px;
        }
        QSlider::add-page:vertical {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #00E5FF, stop:1 #0088FF);
            border-radius: 3px;
        }
        QSlider::handle:vertical {
            background: #FFFFFF;
            border: 2px solid #00E5FF;
            height: 16px;
            margin-left: -5px;
            margin-right: -5px;
            border-radius: 8px;
        }
        QSlider::handle:vertical:hover {
            background: #00E5FF;
            border: 2px solid #FFFFFF;
        }
    )");

    // Frequency Label (e.g. 1 kHz)
    freqLabel_ = new QLabel(freqText, this);
    freqLabel_->setAlignment(Qt::AlignCenter);
    freqLabel_->setStyleSheet("color: #A0A0B0; font-size: 11px; font-weight: 500;");

    layout->addWidget(valueLabel_);
    layout->addWidget(slider_, 1, Qt::AlignHCenter);
    layout->addWidget(freqLabel_);

    updateGainLabel(initialGaindB);

    connect(slider_, &QSlider::valueChanged, this, [this](int value) {
        float gaindB = static_cast<float>(value) / 10.0f;
        updateGainLabel(gaindB);
        emit gainChanged(gaindB);
    });
}

void CustomSlider::setGaindB(float gaindB) {
    slider_->blockSignals(true);
    slider_->setValue(static_cast<int>(gaindB * 10.0f));
    updateGainLabel(gaindB);
    slider_->blockSignals(false);
}

float CustomSlider::gaindB() const {
    return static_cast<float>(slider_->value()) / 10.0f;
}

void CustomSlider::setFrequencyText(const QString& text) {
    freqLabel_->setText(text);
}

void CustomSlider::updateGainLabel(float gaindB) {
    if (gaindB > 0.05f) {
        valueLabel_->setText(QString("+%1").arg(gaindB, 0, 'f', 1));
        valueLabel_->setStyleSheet("color: #00E5FF; font-size: 10px; font-weight: bold;");
    } else if (gaindB < -0.05f) {
        valueLabel_->setText(QString("%1").arg(gaindB, 0, 'f', 1));
        valueLabel_->setStyleSheet("color: #FF5252; font-size: 10px; font-weight: bold;");
    } else {
        valueLabel_->setText("0.0");
        valueLabel_->setStyleSheet("color: #A0A0B0; font-size: 10px; font-weight: bold;");
    }
}

} // namespace eqmax::ui
