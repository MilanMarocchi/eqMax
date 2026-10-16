#include "PowerButton.hpp"
#include "Theme.hpp"
#include <QPainter>

namespace eqmax::ui {

PowerButton::PowerButton(QWidget* parent)
    : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
}

void PowerButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const QColor glyph = isChecked() ? theme::Accent : theme::TextTertiary;

    p.setPen(QPen(isChecked() ? theme::AccentDim : theme::CardBorder, 1.0));
    p.setBrush(underMouse() ? theme::ControlHover : theme::Control);
    p.drawEllipse(r);

    const QRectF glyphRect = r.adjusted(8, 8, -8, -8);
    QPen pen(glyph, 2.0, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawArc(glyphRect, 120 * 16, 300 * 16);
    p.drawLine(QPointF(glyphRect.center().x(), glyphRect.top() - 1.5), QPointF(glyphRect.center().x(), glyphRect.center().y()));
}

} // namespace eqmax::ui
