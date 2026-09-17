#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <memory>
#include "../dsp/EqualizerEngine.hpp"

namespace eqmax::ui {

class BasicEQWidget : public QWidget {
    Q_OBJECT
public:
    explicit BasicEQWidget(std::shared_ptr<dsp::EqualizerEngine> eqEngine, QWidget* parent = nullptr);
    ~BasicEQWidget() override = default;

    void updateValues();

private:
    QWidget* createBandCard(const QString& title, const QString& freqText, size_t bandIdx);

    std::shared_ptr<dsp::EqualizerEngine> eqEngine_;

    QSlider* bassSlider_{nullptr};
    QSlider* midSlider_{nullptr};
    QSlider* trebleSlider_{nullptr};

    QLabel* bassLabel_{nullptr};
    QLabel* midLabel_{nullptr};
    QLabel* trebleLabel_{nullptr};
};

} // namespace eqmax::ui
