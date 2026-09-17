#pragma once

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <vector>
#include <memory>
#include "../dsp/SpectrumAnalyzer.hpp"

namespace eqmax::ui {

class SpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumWidget(std::shared_ptr<dsp::SpectrumAnalyzer> analyzer, QWidget* parent = nullptr);
    ~SpectrumWidget() override;

    void setNumBands(size_t bands);
    void setRefreshRate(int fps = 60);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::shared_ptr<dsp::SpectrumAnalyzer> analyzer_;
    QTimer updateTimer_;
    size_t numBands_{32};
    std::vector<float> peakHold_;
};

} // namespace eqmax::ui
