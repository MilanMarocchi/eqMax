#include "BasicEQWidget.hpp"
#include <QHBoxLayout>
#include <cmath>

namespace eqmax::ui {

BasicEQWidget::BasicEQWidget(std::shared_ptr<dsp::EqualizerEngine> eqEngine, QWidget* parent)
    : QWidget(parent), eqEngine_(std::move(eqEngine)) {

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(0);

    const std::array<QString, 3> labels = { "Bass", "Mid", "Treble" };
    for (size_t i = 0; i < knobs_.size(); ++i) {
        auto* knob = new KnobWidget(labels[i], -12.0, 12.0, 0.0, this);
        knob->setKnobDiameter(72);
        knob->setValueFormatter([](double v) {
            if (std::abs(v) < 0.05) return QString("0 dB");
            return QString("%1%2 dB").arg(v > 0 ? "+" : "").arg(v, 0, 'f', 1);
        });
        connect(knob, &KnobWidget::valueChanged, this, [this, i](double value) {
            if (!eqEngine_) return;
            if (eqEngine_->mode() != dsp::EQMode::ThreeBand) {
                eqEngine_->setMode(dsp::EQMode::ThreeBand);
            }
            eqEngine_->setBandGain(i, static_cast<float>(value));
            emit gainsChanged();
        });
        layout->addStretch(1);
        layout->addWidget(knob);
        knobs_[i] = knob;
    }
    layout->addStretch(1);

    updateValues();
}

void BasicEQWidget::updateValues() {
    if (!eqEngine_ || eqEngine_->mode() != dsp::EQMode::ThreeBand) return;
    for (size_t i = 0; i < knobs_.size(); ++i) {
        knobs_[i]->setValue(eqEngine_->bandGain(i));
    }
}

} // namespace eqmax::ui
