#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include "EqualizerEngine.hpp"
#include "ProEffectsEngine.hpp"
#include "SpectrumAnalyzer.hpp"

namespace eqmax::audio {

struct AudioDeviceInfo {
    uint32_t id{0};
    std::string uid;
    std::string name;
    bool isInput{false};
    bool isOutput{false};
    uint32_t sampleRate{44100};
};

enum class TestSignalType {
    None,
    Sine1kHz,
    PinkNoise,
    SineSweep
};

class AudioDeviceManager {
public:
    AudioDeviceManager(
        std::shared_ptr<dsp::EqualizerEngine> eqEngine,
        std::shared_ptr<dsp::ProEffectsEngine> effectsEngine,
        std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer
    );
    ~AudioDeviceManager();

    /**
     * @brief Query system for active macOS CoreAudio input and output devices.
     */
    std::vector<AudioDeviceInfo> getAudioDevices();

    /**
     * @brief Request macOS Microphone / System Audio Recording permissions via AVFoundation.
     */
    void requestAudioPermissions();

    /**
     * @brief Set target input/output devices.
     */
    bool setOutputDevice(uint32_t deviceId);
    bool setInputDevice(uint32_t deviceId);

    /**
     * @brief Start audio stream IO processing callback loop.
     */
    bool start();

    /**
     * @brief Stop audio stream IO processing callback loop.
     */
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t currentSampleRate() const noexcept { return sampleRate_; }

    // Built-in audio generator control
    void setTestSignalType(TestSignalType type) noexcept { testSignalType_.store(type, std::memory_order_relaxed); }
    [[nodiscard]] TestSignalType testSignalType() const noexcept { return testSignalType_.load(std::memory_order_relaxed); }

    /**
     * @brief Process real-time frame block (called by CoreAudio IOProc callback or internal test generator).
     */
    void processAudioBlock(float* left, float* right, size_t numFrames) noexcept;

private:
    void generateTestSignal(float* left, float* right, size_t numFrames) noexcept;

    std::shared_ptr<dsp::EqualizerEngine> eqEngine_;
    std::shared_ptr<dsp::ProEffectsEngine> effectsEngine_;
    std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer_;

    std::atomic<bool> isRunning_{false};
    uint32_t sampleRate_{44100};
    uint32_t outputDeviceId_{0};
    uint32_t inputDeviceId_{0};

    std::atomic<TestSignalType> testSignalType_{TestSignalType::None};
    float sinePhase_{0.0f};
    float sweepPhase_{0.0f};
    uint32_t pinkNoiseState_{0x12345678};

    // CoreAudio OS X handle pointers
    uintptr_t audioProcHandle_{0};
    uintptr_t audioInputProcHandle_{0};
};

} // namespace eqmax::audio
