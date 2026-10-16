#pragma once

#include <QWidget>
#include <QString>

namespace eqmax::ui {

/**
 * @brief eqMac-style vertical EQ band fader (-12 dB .. +12 dB) with the fill drawn from the 0 dB line.
 * Drag to adjust (Shift for fine control), scroll, or double-click to reset.
 */
class CustomSlider : public QWidget {
    Q_OBJECT
public:
    explicit CustomSlider(const QString& freqLabel, float initialGaindB = 0.0f, QWidget* parent = nullptr);
    ~CustomSlider() override = default;

    void setGaindB(float gaindB);
    [[nodiscard]] float gaindB() const;

    void setFrequencyText(const QString& text);
    void setCompact(bool compact);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void gainChanged(float gaindB);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    [[nodiscard]] QRectF trackRect() const;
    [[nodiscard]] float gainAtY(double y) const;
    void applyGain(float gaindB);

    QString freqText_;
    float gaindB_{0.0f};
    bool compact_{false};
    bool dragging_{false};
    bool fineDrag_{false};
    double dragStartY_{0.0};
    float dragStartGain_{0.0f};
};

} // namespace eqmax::ui
