#pragma once

#include <QAbstractButton>

namespace eqmax::ui {

/**
 * @brief Round, checkable power toggle drawn with the classic power glyph.
 */
class PowerButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit PowerButton(QWidget* parent = nullptr);
    ~PowerButton() override = default;

    [[nodiscard]] QSize sizeHint() const override { return { 30, 30 }; }

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace eqmax::ui
