#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QScrollArea>
#include <QTimer>
#include <array>
#include <map>
#include <memory>
#include <vector>

#include "../dsp/EqualizerEngine.hpp"
#include "../dsp/ProEffectsEngine.hpp"
#include "../dsp/SpectrumAnalyzer.hpp"
#include "../audio/AudioDeviceManager.hpp"
#include "../audio/AppVolumeManager.hpp"
#include "../preset/PresetManager.hpp"
#include "BasicEQWidget.hpp"
#include "CustomSlider.hpp"
#include "KnobWidget.hpp"
#include "PowerButton.hpp"
#include "SectionCard.hpp"
#include "SpectrumWidget.hpp"
#include "VUMeterWidget.hpp"
#include "EQResponseWidget.hpp"

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
    ~MainWindow() override;

    void loadPresetByName(const std::string& name);
    void setBypass(bool bypassed);
    [[nodiscard]] bool isBypassed() const;

signals:
    void bypassChanged(bool bypassed);
    void presetsChanged();

private:
    struct AppRow {
        QWidget* widget{nullptr};
        QLabel* playingDot{nullptr};
    };

    void setupUI();
    QWidget* createHeader();
    SectionCard* createVolumeCard();
    SectionCard* createEqualizerCard();
    SectionCard* createEffectsCard();
    SectionCard* createApplicationsCard();
    SectionCard* createAnalyzerCard();
    SectionCard* createOutputCard();

    void setEqualizerType(dsp::EQMode mode, bool resetEngine = true);
    void rebuildEQSliders();
    void refreshAppMixerList();
    QWidget* createAppRow(const audio::AppAudioInfo& app);
    void updateDeviceDropdowns();
    void updateStatus();
    void saveCurrentAsUserPreset();
    void setMasterVolumedB(float db);
    preset::Preset currentPreset(const std::string& name = "Custom") const;
    void applyPreset(const preset::Preset& preset);
    void refreshPresetCombo(const QString& selected = {});
    void markPresetModified();
    void importPreset();
    void exportPreset();
    void deleteCurrentPreset();
    void restoreSession();
    void saveSession();
    void scheduleSessionSave();

    std::shared_ptr<dsp::EqualizerEngine> eqEngine_;
    std::shared_ptr<dsp::ProEffectsEngine> effectsEngine_;
    std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer_;
    std::shared_ptr<audio::AudioDeviceManager> audioManager_;
    std::shared_ptr<audio::AppVolumeManager> appVolumeManager_;
    std::shared_ptr<preset::PresetManager> presetManager_;

    // Header
    PowerButton* powerBtn_{nullptr};
    QLabel* statusLabel_{nullptr};

    // Volume
    QSlider* masterVolumeSlider_{nullptr};
    QLabel* masterVolumeLabel_{nullptr};
    QSlider* balanceSlider_{nullptr};
    QLabel* balanceLabel_{nullptr};

    // Equalizer
    std::array<QPushButton*, 3> eqTypeButtons_{};
    QComboBox* presetCombo_{nullptr};
    QStackedWidget* eqStack_{nullptr};
    BasicEQWidget* basicEQ_{nullptr};
    QWidget* sliderContainer_{nullptr};
    QHBoxLayout* sliderLayout_{nullptr};
    std::vector<CustomSlider*> eqSliders_;
    EQResponseWidget* response_{nullptr};
    QString selectedPreset_;
    bool applyingState_{false};

    // Effects
    KnobWidget* bassBoostKnob_{nullptr};
    KnobWidget* trebleBoostKnob_{nullptr};
    KnobWidget* spatialKnob_{nullptr};
    QPushButton* limiterToggleBtn_{nullptr};

    // Applications
    QVBoxLayout* appListLayout_{nullptr};
    QLabel* appEmptyLabel_{nullptr};
    std::map<std::string, AppRow> appRows_;

    // Output
    QComboBox* outputDeviceCombo_{nullptr};
    QComboBox* testSignalCombo_{nullptr};

    QTimer statusTimer_;
    QTimer saveTimer_;
    std::vector<SectionCard*> cards_;
};

} // namespace eqmax::ui
