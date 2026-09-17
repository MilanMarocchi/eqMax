#pragma once

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <memory>
#include "../dsp/SpectrumAnalyzer.hpp"

namespace eqmax::ui {

class VUMeterWidget : public QWidget {
    Q_OBJECT
public:
    explicit VUMeterWidget(std::shared_ptr<dsp::SpectrumAnalyzer> analyzer, QWidget* parent = nullptr);
    ~VUMeterWidget() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::shared_ptr<dsp::SpectrumAnalyzer> analyzer_;
    QTimer timer_;
    float smoothRmsL_{0.0f};
    float smoothRmsR_{0.0f};
    float peakHoldL_{0.0f};
    float peakHoldR_{0.0f};
    bool clipL_{false};
    bool clipR_{false};
    int clipHoldCounterL_{0};
    int clipHoldCounterR_{0};
};

} // namespace eqmax::ui
