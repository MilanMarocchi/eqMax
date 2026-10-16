#pragma once

#include <QWidget>
#include <memory>
#include "../dsp/EqualizerEngine.hpp"

namespace eqmax::ui {

class EQResponseWidget : public QWidget {
    Q_OBJECT
public:
    explicit EQResponseWidget(std::shared_ptr<dsp::EqualizerEngine> engine, QWidget* parent = nullptr);
    void setSampleRate(float sampleRate);
    void refresh();
    QSize sizeHint() const override { return {380, 142}; }

signals:
    void bandEdited(size_t index, float gain);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    QRectF plotRect() const;
    float responseGain(float frequency) const;
    QPointF bandPoint(size_t index) const;
    int hitBand(QPointF position) const;
    void editBand(float gain);

    std::shared_ptr<dsp::EqualizerEngine> engine_;
    float sampleRate_{44100};
    int selected_{-1};
    bool dragging_{false};
    double dragLastY_{0};
    double dragGain_{0};
    dsp::EQMode displayedMode_{dsp::EQMode::TenBand};
    std::vector<dsp::BiquadFilter> responseFilters_;
};

} // namespace eqmax::ui
