#pragma once

#include <QWidget>
#include <QString>
#include <functional>

namespace eqmax::ui {

/**
 * @brief eqMac-style rotary knob. Drag vertically (Shift for fine control), scroll, or double-click to reset.
 */
class KnobWidget : public QWidget {
    Q_OBJECT
public:
    explicit KnobWidget(const QString& label, double minimum, double maximum, double defaultValue,
                        QWidget* parent = nullptr);
    ~KnobWidget() override = default;

    void setValue(double value);
    [[nodiscard]] double value() const noexcept { return value_; }

    void setValueFormatter(std::function<QString(double)> formatter);
    void setKnobDiameter(int diameter);

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void valueChanged(double value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void applyValue(double value);
    [[nodiscard]] double normalized(double value) const noexcept;

    QString label_;
    double minimum_;
    double maximum_;
    double default_;
    double value_;
    int diameter_{58};
    std::function<QString(double)> formatter_;

    bool dragging_{false};
    double dragStartValue_{0.0};
    double dragStartY_{0.0};
    bool hovered_{false};
};

} // namespace eqmax::ui
