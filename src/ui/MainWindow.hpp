#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <memory>
#include <vector>

#include "../dsp/EqualizerEngine.hpp"
#include "../dsp/ProEffectsEngine.hpp"
#include "../dsp/SpectrumAnalyzer.hpp"
#include "../audio/AudioDeviceManager.hpp"
#include "../audio/AppVolumeManager.hpp"
#include "../preset/PresetManager.hpp"
#include "CustomSlider.hpp"
#include "SpectrumWidget.hpp"
#include "VUMeterWidget.hpp"

namespace eqmax::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(
        std::shared_ptr<dsp::EqualizerEngine> eqEngine,
        std::shared_ptr<dsp::ProEffectsEngine> effectsEngine,
        std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer,
        std::shared_ptr<audio::AudioDeviceManager> audioManager,
        std::shared_ptr<audio::AppVolumeManager> appVolumeManager,
        std::shared_ptr<preset::PresetManager> presetManager,
        QWidget* parent = nullptr
    );
    ~MainWindow() override = default;

    void loadPresetByName(const std::string& name);
    void setBypass(bool bypassed);

private:
    void setupUI();
    void setupHeaderBar(QVBoxLayout* mainLayout);
    QWidget* createBasicEQTab();
    QWidget* createAdvancedEQTab();
    QWidget* createProEffectsTab();
    QWidget* createAppMixerTab();
    QWidget* createSpectrumTab();
    QWidget* createDevicesTab();

    void rebuildEQSliders(dsp::EQMode mode);
    void refreshAppMixerList(QVBoxLayout* containerLayout);
    void updateDeviceDropdowns();
    void saveCurrentAsUserPreset();

    std::shared_ptr<dsp::EqualizerEngine> eqEngine_;
    std::shared_ptr<dsp::ProEffectsEngine> effectsEngine_;
    std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer_;
    std::shared_ptr<audio::AudioDeviceManager> audioManager_;
    std::shared_ptr<audio::AppVolumeManager> appVolumeManager_;
    std::shared_ptr<preset::PresetManager> presetManager_;

    // Header Controls
    QPushButton* powerBtn_{nullptr};
    QComboBox* outputDeviceCombo_{nullptr};
    QComboBox* inputDeviceCombo_{nullptr};
    QSlider* masterVolumeSlider_{nullptr};
    QLabel* masterVolumeLabel_{nullptr};

    // Equalizer Tab Controls
    QComboBox* modeCombo_{nullptr};
    QComboBox* presetCombo_{nullptr};
    QWidget* sliderContainer_{nullptr};
    QHBoxLayout* sliderLayout_{nullptr};
    std::vector<CustomSlider*> eqSliders_;

    // Pro Effects Tab Controls
    QSlider* volumeBoostSlider_{nullptr};
    QLabel* volumeBoostLabel_{nullptr};
    QSlider* balanceSlider_{nullptr};
    QSlider* spatialSlider_{nullptr};
    QSlider* bassBoostSlider_{nullptr};
    QSlider* trebleBoostSlider_{nullptr};
    QPushButton* limiterToggleBtn_{nullptr};

    // Diagnostics Controls
    QComboBox* testSignalCombo_{nullptr};

    SpectrumWidget* spectrumWidget_{nullptr};
    VUMeterWidget* vuMeterWidget_{nullptr};
};

} // namespace eqmax::ui
