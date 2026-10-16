#include "VUMeterWidget.hpp"
#include "Theme.hpp"
#include <QLinearGradient>
#include <QPaintEvent>
#include <algorithm>

namespace eqmax::ui {

VUMeterWidget::VUMeterWidget(std::shared_ptr<dsp::SpectrumAnalyzer> analyzer, QWidget* parent)
    : QWidget(parent), analyzer_(std::move(analyzer)) {

    setMinimumWidth(80);
    setMinimumHeight(140);

    connect(&timer_, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    timer_.start(20); // 50 FPS
}

VUMeterWidget::~VUMeterWidget() {
    timer_.stop();
}

void VUMeterWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::Window);
    painter.drawRoundedRect(rect(), 6, 6);

    if (!analyzer_) return;

    auto levels = analyzer_->getLevels();

    // Smooth RMS values
    smoothRmsL_ = smoothRmsL_ * 0.7f + levels.rmsLeft * 0.3f;
    smoothRmsR_ = smoothRmsR_ * 0.7f + levels.rmsRight * 0.3f;

    // Peak tracking
    if (levels.peakLeft > peakHoldL_) {
        peakHoldL_ = levels.peakLeft;
    } else {
        peakHoldL_ *= 0.95f;
    }

    if (levels.peakRight > peakHoldR_) {
        peakHoldR_ = levels.peakRight;
    } else {
        peakHoldR_ *= 0.95f;
    }

    // Clipping LED trigger
    if (levels.peakLeft >= 0.99f) { clipL_ = true; clipHoldCounterL_ = 30; }
    else if (clipHoldCounterL_ > 0) { --clipHoldCounterL_; }
    else { clipL_ = false; }

    if (levels.peakRight >= 0.99f) { clipR_ = true; clipHoldCounterR_ = 30; }
    else if (clipHoldCounterR_ > 0) { --clipHoldCounterR_; }
    else { clipR_ = false; }

    int meterW = (w - 30) / 2;
    int meterH = h - 35;
    int topY = 25;

    // Draw Left & Right Meter Channels
    auto drawChannel = [&](int x, float rms, float peak, bool clip, const QString& label) {
        // Channel Label (L / R)
        painter.setPen(theme::TextSecondary);
        QFont labelFont = font();
        labelFont.setPixelSize(10);
        labelFont.setWeight(QFont::DemiBold);
        painter.setFont(labelFont);
        painter.drawText(QRect(x, 5, meterW, 18), Qt::AlignCenter, label);

        // Meter Track
        painter.fillRect(QRect(x, topY, meterW, meterH), theme::Track.darker(160));

        // RMS Bar
        float normRms = std::clamp(rms * 2.0f, 0.0f, 1.0f);
        int barH = static_cast<int>(normRms * meterH);
        int barY = topY + (meterH - barH);

        QLinearGradient grad(x, topY + meterH, x, topY);
        grad.setColorAt(0.0, theme::AccentDim);
        grad.setColorAt(0.7, theme::Accent);
        grad.setColorAt(1.0, theme::Danger);

        painter.fillRect(QRect(x, barY, meterW, barH), grad);

        // Peak Hold Line
        float normPeak = std::clamp(peak * 2.0f, 0.0f, 1.0f);
        int peakY = topY + (meterH - static_cast<int>(normPeak * meterH));
        painter.setPen(QPen(QColor(255, 255, 255, 180), 1.5));
        painter.drawLine(x, peakY, x + meterW, peakY);

        // Clip Indicator Light
        QColor clipColor = clip ? theme::Danger : theme::Track;
        painter.setBrush(clipColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPoint(x + meterW / 2, topY + meterH + 6), 4, 4);
    };

    drawChannel(10, smoothRmsL_, peakHoldL_, clipL_, "L");
    drawChannel(15 + meterW, smoothRmsR_, peakHoldR_, clipR_, "R");
}

} // namespace eqmax::ui
