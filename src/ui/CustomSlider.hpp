#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QString>

namespace eqmax::ui {

class CustomSlider : public QWidget {
    Q_OBJECT
public:
    explicit CustomSlider(const QString& freqLabel, float initialGaindB = 0.0f, QWidget* parent = nullptr);
    ~CustomSlider() override = default;

    void setGaindB(float gaindB);
    [[nodiscard]] float gaindB() const;

    void setFrequencyText(const QString& text);

signals:
    void gainChanged(float gaindB);

private:
    void updateGainLabel(float gaindB);

    QLabel* valueLabel_{nullptr};
    QSlider* slider_{nullptr};
    QLabel* freqLabel_{nullptr};
};

} // namespace eqmax::ui
