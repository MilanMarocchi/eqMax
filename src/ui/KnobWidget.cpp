#include "KnobWidget.hpp"
#include <QKeyEvent>
#include "Theme.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>

namespace eqmax::ui {

namespace {
constexpr double kStartAngle = 225.0; // degrees, Qt convention (counter-clockwise from 3 o'clock)
constexpr double kSweepAngle = 270.0;
constexpr double kDragPixelsForFullRange = 180.0;
}

KnobWidget::KnobWidget(const QString& label, double minimum, double maximum, double defaultValue, QWidget* parent)
    : QWidget(parent),
      label_(label),
      minimum_(minimum),
      maximum_(maximum),
      default_(std::clamp(defaultValue, minimum, maximum)),
      value_(default_) {
    setCursor(Qt::SizeVerCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(label);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    formatter_ = [](double v) { return QString::number(v, 'f', 1); };
    setToolTip("Drag to adjust, Shift for fine control, double-click to reset");
}

void KnobWidget::setValue(double value) {
    value = std::clamp(value, minimum_, maximum_);
    if (value == value_) return;
    value_ = value;
    update();
}

void KnobWidget::setValueFormatter(std::function<QString(double)> formatter) {
    formatter_ = std::move(formatter);
    update();
}

void KnobWidget::setKnobDiameter(int diameter) {
    diameter_ = diameter;
    updateGeometry();
    update();
}

QSize KnobWidget::sizeHint() const {
    return { diameter_ + 36, diameter_ + 52 };
}

double KnobWidget::normalized(double value) const noexcept {
    if (maximum_ <= minimum_) return 0.0;
    return (value - minimum_) / (maximum_ - minimum_);
}

void KnobWidget::applyValue(double value) {
    value = std::clamp(value, minimum_, maximum_);
    if (std::abs(value - value_) < 1e-9) return;
    value_ = value;
    update();
    emit valueChanged(value_);
}

void KnobWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double cx = width() / 2.0;
    const double top = 4.0;
    const double arcPad = 5.0;
    const QRectF arcRect(cx - diameter_ / 2.0, top, diameter_, diameter_);
    const QRectF bodyRect = arcRect.adjusted(arcPad + 3, arcPad + 3, -(arcPad + 3), -(arcPad + 3));

    // Track arc
    QPen trackPen(theme::Track, 3.0, Qt::SolidLine, Qt::RoundCap);
    p.setPen(trackPen);
    p.drawArc(arcRect.adjusted(1.5, 1.5, -1.5, -1.5), static_cast<int>(kStartAngle * 16), static_cast<int>(-kSweepAngle * 16));

    // Value arc, drawn from the default (e.g. 0 dB center for bipolar knobs)
    const double originAngle = kStartAngle - normalized(default_) * kSweepAngle;
    const double valueAngle = kStartAngle - normalized(value_) * kSweepAngle;
    if (std::abs(valueAngle - originAngle) > 0.5) {
        QPen valuePen(isEnabled() ? theme::Accent : theme::TextTertiary, 3.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(valuePen);
        p.drawArc(arcRect.adjusted(1.5, 1.5, -1.5, -1.5), static_cast<int>(originAngle * 16),
                  static_cast<int>((valueAngle - originAngle) * 16));
    }

    // Knob body
    QRadialGradient body(bodyRect.center() - QPointF(0, bodyRect.height() * 0.25), bodyRect.width() * 0.75);
    body.setColorAt(0.0, hovered_ || dragging_ ? QColor(0x55, 0x55, 0x5B) : QColor(0x4A, 0x4A, 0x50));
    body.setColorAt(1.0, QColor(0x2A, 0x2A, 0x2E));
    p.setPen(QPen(QColor(0, 0, 0, 120), 1.0));
    p.setBrush(body);
    p.drawEllipse(bodyRect);

    // Indicator
    const double radians = valueAngle * M_PI / 180.0;
    const double inner = bodyRect.width() / 2.0 * 0.35;
    const double outer = bodyRect.width() / 2.0 * 0.78;
    const QPointF c = bodyRect.center();
    QPen indicatorPen(QColor(0xF2, 0xF2, 0xF4), 2.5, Qt::SolidLine, Qt::RoundCap);
    p.setPen(indicatorPen);
    p.drawLine(QPointF(c.x() + inner * std::cos(radians), c.y() - inner * std::sin(radians)),
               QPointF(c.x() + outer * std::cos(radians), c.y() - outer * std::sin(radians)));

    // Label and value
    QFont labelFont = font();
    labelFont.setPixelSize(11);
    labelFont.setWeight(QFont::DemiBold);
    p.setFont(labelFont);
    p.setPen(theme::TextSecondary);
    const double textTop = top + diameter_ + 4;
    p.drawText(QRectF(0, textTop, width(), 16), Qt::AlignCenter, label_);

    QFont valueFont = font();
    valueFont.setPixelSize(11);
    p.setFont(valueFont);
    p.setPen(dragging_ ? theme::Accent : theme::TextPrimary);
    p.drawText(QRectF(0, textTop + 16, width(), 16), Qt::AlignCenter, formatter_ ? formatter_(value_) : QString());
}

void KnobWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    dragging_ = true;
    dragStartValue_ = value_;
    dragStartY_ = event->position().y();
    update();
}

void KnobWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) return;
    double pixels = kDragPixelsForFullRange * ((event->modifiers() & Qt::ShiftModifier) ? 5.0 : 1.0);
    double delta = (dragStartY_ - event->position().y()) / pixels * (maximum_ - minimum_);
    applyValue(dragStartValue_ + delta);
}

void KnobWidget::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    update();
}

void KnobWidget::mouseDoubleClickEvent(QMouseEvent*) {
    applyValue(default_);
}

void KnobWidget::wheelEvent(QWheelEvent* event) {
    const double step = (maximum_ - minimum_) / 100.0;
    const double notches = event->angleDelta().y() / 120.0;
    applyValue(value_ + notches * step * ((event->modifiers() & Qt::ShiftModifier) ? 0.2 : 1.0));
    event->accept();
}

void KnobWidget::keyPressEvent(QKeyEvent* event) {
    const double step = (maximum_ - minimum_) / ((event->modifiers() & Qt::ShiftModifier) ? 240.0 : 48.0);
    switch (event->key()) {
        case Qt::Key_Up: case Qt::Key_Right: applyValue(value_ + step); break;
        case Qt::Key_Down: case Qt::Key_Left: applyValue(value_ - step); break;
        case Qt::Key_Home: applyValue(default_); break;
        default: QWidget::keyPressEvent(event); return;
    }
    event->accept();
}

void KnobWidget::enterEvent(QEnterEvent*) {
    hovered_ = true;
    update();
}

void KnobWidget::leaveEvent(QEvent*) {
    hovered_ = false;
    update();
}

} // namespace eqmax::ui
