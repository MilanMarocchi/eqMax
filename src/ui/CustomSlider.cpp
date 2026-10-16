#include "CustomSlider.hpp"
#include "Theme.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>

namespace eqmax::ui {

namespace {
constexpr float kMinGain = -12.0f;
constexpr float kMaxGain = 12.0f;
constexpr double kValueTextHeight = 16.0;
constexpr double kFreqTextHeight = 16.0;
constexpr double kHandleRadius = 7.0;
}

CustomSlider::CustomSlider(const QString& freqText, float initialGaindB, QWidget* parent)
    : QWidget(parent), freqText_(freqText), gaindB_(std::clamp(initialGaindB, kMinGain, kMaxGain)) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(freqText + " Hz equalizer gain");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setToolTip(QString("%1 Hz band. Double-click to reset").arg(freqText));
}

QSize CustomSlider::sizeHint() const {
    return { compact_ ? 26 : 36, 190 };
}

QSize CustomSlider::minimumSizeHint() const {
    return { compact_ ? 24 : 30, 140 };
}

void CustomSlider::setCompact(bool compact) {
    compact_ = compact;
    updateGeometry();
    update();
}

void CustomSlider::setGaindB(float gaindB) {
    gaindB_ = std::clamp(gaindB, kMinGain, kMaxGain);
    update();
}

float CustomSlider::gaindB() const {
    return gaindB_;
}

void CustomSlider::setFrequencyText(const QString& text) {
    freqText_ = text;
    update();
}

QRectF CustomSlider::trackRect() const {
    const double top = kValueTextHeight + kHandleRadius + 2;
    const double bottom = height() - kFreqTextHeight - kHandleRadius - 2;
    return { width() / 2.0 - 2.0, top, 4.0, std::max(10.0, bottom - top) };
}

float CustomSlider::gainAtY(double y) const {
    const QRectF track = trackRect();
    const double t = std::clamp((track.bottom() - y) / track.height(), 0.0, 1.0);
    return static_cast<float>(kMinGain + t * (kMaxGain - kMinGain));
}

void CustomSlider::applyGain(float gaindB) {
    gaindB = std::clamp(std::round(gaindB * 10.0f) / 10.0f, kMinGain, kMaxGain);
    if (gaindB == gaindB_) return;
    gaindB_ = gaindB;
    update();
    emit gainChanged(gaindB_);
}

void CustomSlider::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = trackRect();
    const auto yForGain = [&](float g) {
        return track.bottom() - (g - kMinGain) / (kMaxGain - kMinGain) * track.height();
    };

    // Track
    p.setPen(Qt::NoPen);
    p.setBrush(theme::Track);
    p.drawRoundedRect(track, 2, 2);

    // Zero line tick
    const double zeroY = yForGain(0.0f);
    p.setPen(QPen(theme::TextTertiary, 1.0));
    p.drawLine(QPointF(track.left() - 2, zeroY), QPointF(track.right() + 2, zeroY));

    // Fill from 0 dB to value
    const double valueY = yForGain(gaindB_);
    if (std::abs(gaindB_) >= 0.05f) {
        p.setPen(Qt::NoPen);
        p.setBrush(theme::Accent);
        p.drawRoundedRect(QRectF(track.left(), std::min(zeroY, valueY), track.width(), std::abs(zeroY - valueY)), 2, 2);
    }

    // Handle
    const QPointF handleCenter(track.center().x(), valueY);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
    p.setBrush(dragging_ ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xE6, 0xE6, 0xEA));
    p.drawEllipse(handleCenter, kHandleRadius, kHandleRadius);

    // Value (top) and frequency (bottom)
    QFont f = font();
    f.setPixelSize(compact_ ? 9 : 10);
    p.setFont(f);
    const QString valueText = std::abs(gaindB_) < 0.05f ? QString("0")
                              : QString("%1%2").arg(gaindB_ > 0 ? "+" : "").arg(gaindB_, 0, 'f', compact_ ? 0 : 1);
    p.setPen(std::abs(gaindB_) < 0.05f ? theme::TextTertiary : theme::TextPrimary);
    p.drawText(QRectF(0, 0, width(), kValueTextHeight), Qt::AlignCenter, valueText);

    p.setPen(theme::TextSecondary);
    p.drawText(QRectF(0, height() - kFreqTextHeight, width(), kFreqTextHeight), Qt::AlignCenter, freqText_);
}

void CustomSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    dragging_ = true;
    fineDrag_ = event->modifiers() & Qt::ShiftModifier;
    dragStartY_ = event->position().y();
    dragStartGain_ = gaindB_;
    if (!fineDrag_) applyGain(gainAtY(event->position().y()));
    update();
}

void CustomSlider::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;
    if (fineDrag_) {
        const double dbPerPixel = (kMaxGain - kMinGain) / trackRect().height() / 5.0;
        applyGain(dragStartGain_ + static_cast<float>((dragStartY_ - event->position().y()) * dbPerPixel));
    } else {
        applyGain(gainAtY(event->position().y()));
    }
}

void CustomSlider::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    update();
}

void CustomSlider::mouseDoubleClickEvent(QMouseEvent*) {
    applyGain(0.0f);
}

void CustomSlider::wheelEvent(QWheelEvent* event) {
    const float notches = static_cast<float>(event->angleDelta().y()) / 120.0f;
    applyGain(gaindB_ + notches * ((event->modifiers() & Qt::ShiftModifier) ? 0.1f : 0.5f));
    event->accept();
}

void CustomSlider::keyPressEvent(QKeyEvent* event) {
    const float step = event->modifiers() & Qt::ShiftModifier ? 0.1f : 0.5f;
    switch (event->key()) {
        case Qt::Key_Up: case Qt::Key_Right: applyGain(gaindB_ + step); break;
        case Qt::Key_Down: case Qt::Key_Left: applyGain(gaindB_ - step); break;
        case Qt::Key_Home: applyGain(0); break;
        default: QWidget::keyPressEvent(event); return;
    }
    event->accept();
}

} // namespace eqmax::ui
