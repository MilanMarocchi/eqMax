#include "SpectrumWidget.hpp"
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

void SpectrumWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // 1. Dark Glass Background
    painter.fillRect(rect(), QColor(24, 24, 30));

    // 2. Draw Subtle Frequency Grid Lines (-12dB, -24dB, -36dB, -48dB)
    painter.setPen(QPen(QColor(255, 255, 255, 15), 1, Qt::DashLine));
    for (int db = 1; db < 4; ++db) {
        int y = h * db / 4;
        painter.drawLine(0, y, w, y);
    }

    if (!analyzer_) return;

    // 3. Retrieve spectrum energy levels
    auto energies = analyzer_->getBandEnergies(numBands_);
    if (energies.empty()) return;

    const float barSpacing = 2.0f;
    const float totalSpacing = barSpacing * (numBands_ + 1);
    const float barWidth = (w - totalSpacing) / static_cast<float>(numBands_);

    for (size_t i = 0; i < numBands_; ++i) {
        float val = std::clamp(energies[i], 0.0f, 1.0f);
        
        // Update Peak Hold
        if (val > peakHold_[i]) {
            peakHold_[i] = val;
        } else {
            peakHold_[i] *= 0.96f; // Peak gravity drop
        }

        float x = barSpacing + i * (barWidth + barSpacing);
        float barHeight = val * (h - 20);
        float y = h - 20 - barHeight;

        // 4. Create Vibrant Gradient for Spectrum Bars
        QLinearGradient grad(x, h - 20, x, y);
        grad.setColorAt(0.0, QColor(0, 229, 255, 220));   // Bright Cyan at bottom
        grad.setColorAt(0.7, QColor(0, 136, 255, 240));   // Deep Blue in middle
        grad.setColorAt(1.0, QColor(180, 0, 255, 255));   // Electric Purple at top

        painter.fillRect(QRectF(x, y, barWidth, barHeight), grad);

        // 5. Draw Peak Hold Line
        float peakY = h - 20 - (peakHold_[i] * (h - 20));
        painter.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
        painter.drawLine(QPointF(x, peakY), QPointF(x + barWidth, peakY));
    }

    // 6. Draw Frequency Legend at bottom
    painter.setPen(QColor(160, 160, 180));
    QFont font("sans-serif", 9);
    painter.setFont(font);
    painter.drawText(QRect(5, h - 18, 60, 16), Qt::AlignLeft, "20 Hz");
    painter.drawText(QRect(w / 4 - 30, h - 18, 60, 16), Qt::AlignCenter, "250 Hz");
    painter.drawText(QRect(w / 2 - 30, h - 18, 60, 16), Qt::AlignCenter, "1 kHz");
    painter.drawText(QRect(3 * w / 4 - 30, h - 18, 60, 16), Qt::AlignCenter, "8 kHz");
    painter.drawText(QRect(w - 65, h - 18, 60, 16), Qt::AlignRight, "20 kHz");
}

} // namespace eqmax::ui
