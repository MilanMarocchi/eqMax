#include "EQResponseWidget.hpp"
#include "Theme.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace eqmax::ui {

EQResponseWidget::EQResponseWidget(std::shared_ptr<dsp::EqualizerEngine> engine, QWidget* parent)
    : QWidget(parent), engine_(std::move(engine)) {
    setObjectName("eqResponse");
    setAccessibleName("Equalizer frequency response");
    setAccessibleDescription("Drag band points to adjust gain. Left and right select bands, up and down adjust gain, Home resets.");
    setToolTip("Drag a point to shape your sound · Double-click to reset a band");
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setFixedHeight(sizeHint().height());
    refresh();
}

void EQResponseWidget::setSampleRate(float sampleRate) {
    if (sampleRate >= 8000 && sampleRate != sampleRate_) { sampleRate_ = sampleRate; refresh(); }
}

void EQResponseWidget::refresh() {
    responseFilters_.clear();
    if (engine_) {
        if (displayedMode_ != engine_->mode()) {
            displayedMode_ = engine_->mode();
            selected_ = -1;
            dragging_ = false;
        }
        if (selected_ >= static_cast<int>(engine_->numBands())) selected_ = -1;
        const float q = displayedMode_ == dsp::EQMode::ThreeBand ? 0.707f :
                        displayedMode_ == dsp::EQMode::TenBand ? 1.414f : 4.318f;
        for (const auto& band : engine_->bandInfos()) {
            if (std::abs(band.gaindB) < 0.01f) continue;
            dsp::BiquadFilter filter;
            filter.configure(band.type, band.frequency, sampleRate_, band.gaindB, q);
            responseFilters_.push_back(filter);
        }
    }
    update();
}

QRectF EQResponseWidget::plotRect() const { return QRectF(rect()).adjusted(30, 12, -10, -22); }

float EQResponseWidget::responseGain(float frequency) const {
    float gain = 0;
    for (const auto& filter : responseFilters_) gain += filter.magnitudedB(frequency);
    return gain;
}

QPointF EQResponseWidget::bandPoint(size_t index) const {
    const auto& band = engine_->bandInfos()[index];
    const auto plot = plotRect();
    return {plot.left() + std::log10(band.frequency / 20.0) / 3.0 * plot.width(),
            plot.center().y() - std::clamp(responseGain(band.frequency), -18.0f, 18.0f) / 18.0 * plot.height() / 2.0};
}

void EQResponseWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(theme::Window);
    p.drawRoundedRect(rect(), 7, 7);
    const auto plot = plotRect();
    QFont f = font(); f.setPixelSize(9); p.setFont(f);
    for (int gain : {-12, 0, 12}) {
        const double y = plot.center().y() - gain / 18.0 * plot.height() / 2.0;
        p.setPen(QPen(gain == 0 ? theme::Track : theme::CardBorder, 1, Qt::DashLine));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(theme::TextSecondary);
        p.drawText(QRectF(0, y - 7, 24, 14), Qt::AlignRight | Qt::AlignVCenter, gain > 0 ? "+12" : QString::number(gain));
    }
    for (int frequency : {20, 100, 1000, 10000, 20000}) {
        const double x = plot.left() + std::log10(frequency / 20.0) / 3.0 * plot.width();
        p.setPen(QPen(theme::CardBorder, 1));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(theme::TextSecondary);
        p.drawText(QRectF(x - 17, plot.bottom() + 5, 34, 14), Qt::AlignCenter,
                   frequency >= 1000 ? QString("%1k").arg(frequency / 1000) : QString::number(frequency));
    }
    if (!engine_) return;
    QPainterPath curve;
    for (int i = 0; i <= 256; ++i) {
        const float frequency = 20.0f * std::pow(1000.0f, i / 256.0f);
        const float gain = responseGain(frequency);
        const QPointF point(plot.left() + i / 256.0 * plot.width(),
                            plot.center().y() - gain / 18.0 * plot.height() / 2.0);
        if (i == 0) curve.moveTo(point); else curve.lineTo(point);
    }
    auto fill = curve;
    fill.lineTo(plot.right(), plot.center().y());
    fill.lineTo(plot.left(), plot.center().y()); fill.closeSubpath();
    const QColor accent = isEnabled() ? theme::Accent : theme::TextTertiary;
    QColor wash = accent; wash.setAlpha(30);
    p.save();
    p.setClipRect(plot);
    p.fillPath(fill, wash);
    // The response is an open stroke. A brush would implicitly close it with a diagonal
    // and paint over the zero-anchored fill, creating wedges and missing blue areas.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(accent, 1.8)); p.drawPath(curve);
    p.restore();
    for (size_t i = 0; i < engine_->numBands(); ++i) {
        p.setPen(QPen(accent, 1.5));
        p.setBrush(static_cast<int>(i) == selected_ ? accent : theme::Card);
        const double radius = static_cast<int>(i) == selected_ && hasFocus() ? 5 : engine_->numBands() > 10 ? 2.5 : 3.5;
        p.drawEllipse(bandPoint(i), radius, radius);
    }
}

int EQResponseWidget::hitBand(QPointF position) const {
    if (!engine_) return -1;
    int closest = -1;
    double distance = 15;
    for (size_t i = 0; i < engine_->numBands(); ++i) {
        const double d = QLineF(position, bandPoint(i)).length();
        if (d < distance) { closest = static_cast<int>(i); distance = d; }
    }
    return closest;
}

void EQResponseWidget::editBand(float gain) {
    if (!engine_ || selected_ < 0 || selected_ >= static_cast<int>(engine_->numBands())) return;
    gain = std::clamp(std::round(gain * 10) / 10, -12.0f, 12.0f);
    if (gain == engine_->bandGain(static_cast<size_t>(selected_))) return;
    emit bandEdited(static_cast<size_t>(selected_), gain);
    update();
}

void EQResponseWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    selected_ = hitBand(event->position());
    dragging_ = selected_ >= 0;
    if (dragging_) {
        dragLastY_ = event->globalPosition().y();
        dragGain_ = engine_->bandGain(static_cast<size_t>(selected_));
        QToolTip::hideText();
    }
    setFocus(); update();
}

void EQResponseWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        // Handles sit on the combined response, while dragging edits this band's gain.
        // Preserve the grab offset and use deltas so neighbouring filters don't cause jumps.
        const double sensitivity = event->modifiers() & Qt::ShiftModifier ? 0.2 : 1.0;
        const double delta = (dragLastY_ - event->globalPosition().y()) * 36 / plotRect().height() * sensitivity;
        dragLastY_ = event->globalPosition().y();
        dragGain_ = std::clamp(dragGain_ + delta, -12.0, 12.0);
        editBand(static_cast<float>(dragGain_));
    } else {
        const int index = hitBand(event->position());
        setCursor(index >= 0 ? Qt::SizeVerCursor : Qt::ArrowCursor);
        if (index >= 0) {
            const auto& band = engine_->bandInfos()[index];
            QToolTip::showText(event->globalPosition().toPoint(), QString("%1 Hz · %2 dB").arg(band.frequency).arg(band.gaindB, 0, 'f', 1), this);
        } else QToolTip::hideText();
    }
}

void EQResponseWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) { dragging_ = false; update(); }
}
void EQResponseWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    selected_ = hitBand(event->position()); editBand(0);
}

void EQResponseWidget::keyPressEvent(QKeyEvent* event) {
    if (!engine_ || !engine_->numBands()) return;
    if (selected_ < 0) selected_ = 0;
    const float step = event->modifiers() & Qt::ShiftModifier ? 0.1f : 0.5f;
    switch (event->key()) {
        case Qt::Key_Left: selected_ = std::max(0, selected_ - 1); break;
        case Qt::Key_Right: selected_ = std::min(static_cast<int>(engine_->numBands()) - 1, selected_ + 1); break;
        case Qt::Key_Up: editBand(engine_->bandGain(selected_) + step); break;
        case Qt::Key_Down: editBand(engine_->bandGain(selected_) - step); break;
        case Qt::Key_Home: editBand(0); break;
        default: QWidget::keyPressEvent(event); return;
    }
    event->accept(); update();
}

} // namespace eqmax::ui
