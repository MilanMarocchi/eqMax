#include "SpectrumWidget.hpp"
#include "Theme.hpp"
#include <QLinearGradient>
#include <QPaintEvent>
#include <algorithm>

namespace eqmax::ui {

SpectrumWidget::SpectrumWidget(std::shared_ptr<dsp::SpectrumAnalyzer> analyzer, QWidget* parent)
    : QWidget(parent), analyzer_(std::move(analyzer)) {

    setMinimumHeight(140);
    peakHold_.assign(numBands_, 0.0f);

    connect(&updateTimer_, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    updateTimer_.start(16); // ~60 FPS
}

SpectrumWidget::~SpectrumWidget() {
    updateTimer_.stop();
}

void SpectrumWidget::setNumBands(size_t bands) {
    numBands_ = std::max<size_t>(8, bands);
    peakHold_.assign(numBands_, 0.0f);
}

void SpectrumWidget::setRefreshRate(int fps) {
    int interval = 1000 / std::clamp(fps, 10, 120);
    updateTimer_.setInterval(interval);
}

void SpectrumWidget::setShowLegend(bool show) {
    showLegend_ = show;
    update();
}

void SpectrumWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int legendHeight = showLegend_ ? 18 : 0;
    const float plotHeight = static_cast<float>(h - legendHeight);

    // Background panel
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::Window);
    painter.drawRoundedRect(rect(), 6, 6);

    // Subtle level grid
    painter.setPen(QPen(QColor(255, 255, 255, 12), 1));
    for (int i = 1; i < 4; ++i) {
        const int y = static_cast<int>(plotHeight * i / 4);
        painter.drawLine(0, y, w, y);
    }

    if (!analyzer_) return;

    auto energies = analyzer_->getBandEnergies(numBands_);
    if (energies.empty()) return;

    const float barSpacing = 2.0f;
    const float totalSpacing = barSpacing * (numBands_ + 1);
    const float barWidth = std::max(1.0f, (w - totalSpacing) / static_cast<float>(numBands_));
    const float usableHeight = plotHeight - 4.0f;

    for (size_t i = 0; i < numBands_ && i < energies.size(); ++i) {
        const float val = std::clamp(energies[i], 0.0f, 1.0f);
        peakHold_[i] = val > peakHold_[i] ? val : peakHold_[i] * 0.96f;

        const float x = barSpacing + i * (barWidth + barSpacing);
        const float barHeight = std::max(1.0f, val * usableHeight);
        const float y = plotHeight - barHeight;

        QLinearGradient grad(x, plotHeight, x, 0);
        grad.setColorAt(0.0, theme::AccentDim);
        grad.setColorAt(1.0, theme::Accent);
        painter.setPen(Qt::NoPen);
        painter.setBrush(grad);
        painter.drawRoundedRect(QRectF(x, y, barWidth, barHeight), 1.0, 1.0);

        const float peakY = plotHeight - peakHold_[i] * usableHeight;
        painter.setPen(QPen(QColor(255, 255, 255, 110), 1.0));
        painter.drawLine(QPointF(x, peakY), QPointF(x + barWidth, peakY));
    }

    if (showLegend_) {
        painter.setPen(theme::TextSecondary);
        QFont legendFont = font();
        legendFont.setPixelSize(10);
        painter.setFont(legendFont);
        const int textY = h - legendHeight;
        painter.drawText(QRect(6, textY, 60, legendHeight), Qt::AlignLeft | Qt::AlignVCenter, "20 Hz");
        painter.drawText(QRect(w / 4 - 30, textY, 60, legendHeight), Qt::AlignCenter, "250 Hz");
        painter.drawText(QRect(w / 2 - 30, textY, 60, legendHeight), Qt::AlignCenter, "1 kHz");
        painter.drawText(QRect(3 * w / 4 - 30, textY, 60, legendHeight), Qt::AlignCenter, "8 kHz");
        painter.drawText(QRect(w - 66, textY, 60, legendHeight), Qt::AlignRight | Qt::AlignVCenter, "20 kHz");
    }
}

} // namespace eqmax::ui
