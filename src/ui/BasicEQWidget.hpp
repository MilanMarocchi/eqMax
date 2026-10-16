#pragma once

#include <QWidget>
#include <array>
#include <memory>
#include "../dsp/EqualizerEngine.hpp"
#include "KnobWidget.hpp"

namespace eqmax::ui {

/**
 * @brief eqMac-style "Basic" equalizer: Bass / Mid / Treble knobs driving the 3-band EQ mode.
 */
class BasicEQWidget : public QWidget {
    Q_OBJECT
public:
    explicit BasicEQWidget(std::shared_ptr<dsp::EqualizerEngine> eqEngine, QWidget* parent = nullptr);
    ~BasicEQWidget() override = default;

    /**
     * @brief Refresh knob positions from the engine (when it is in 3-band mode).
     */
    void updateValues();

signals:
    void gainsChanged();

private:
    std::shared_ptr<dsp::EqualizerEngine> eqEngine_;
    std::array<KnobWidget*, 3> knobs_{};
};

} // namespace eqmax::ui
