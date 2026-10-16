#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include "EqualizerEngine.hpp"
#include "ProEffectsEngine.hpp"
#include "SpectrumAnalyzer.hpp"
#include "AppVolumeManager.hpp"

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

/**
 * @brief Routes all system audio through the DSP chain.
 *
 * Uses Core Audio process taps (macOS 14.2+): every other process's output is captured and muted,
 * mixed with per-application gain, processed (EQ, effects, analyzer) and rendered to the real
 * output device through a private aggregate device.
 */
class AudioDeviceManager {
public:
    AudioDeviceManager(
        std::shared_ptr<dsp::EqualizerEngine> eqEngine,
        std::shared_ptr<dsp::ProEffectsEngine> effectsEngine,
        std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer,
        std::shared_ptr<AppVolumeManager> appVolumeManager
    );
    ~AudioDeviceManager();

    /**
     * @brief Query system for active macOS CoreAudio input and output devices.
     */
    std::vector<AudioDeviceInfo> getAudioDevices();

    /**
     * @brief Open the macOS "Screen & System Audio Recording" privacy settings.
     * System audio capture permission is prompted automatically the first time routing starts.
     */
    void requestAudioPermissions();

    /**
     * @brief Set target output device (0 = follow the system default output device).
     */
    bool setOutputDevice(uint32_t deviceId);

    /**
     * @brief Start capturing system audio and routing it through the DSP chain.
     */
    bool start();

    /**
     * @brief Stop routing; applications play directly to their devices again.
     */
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isRoutingActive() const noexcept { return routingActive_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t currentSampleRate() const noexcept { return sampleRate_.load(std::memory_order_relaxed); }

    // Built-in audio generator control
    void setTestSignalType(TestSignalType type) noexcept { testSignalType_.store(type, std::memory_order_relaxed); }
    [[nodiscard]] TestSignalType testSignalType() const noexcept { return testSignalType_.load(std::memory_order_relaxed); }

    /**
     * @brief Process real-time frame block (called by CoreAudio IOProc callback).
     */
    void processAudioBlock(float* left, float* right, size_t numFrames) noexcept;

    struct Impl;

private:
    void generateTestSignal(float* left, float* right, size_t numFrames) noexcept;

    std::shared_ptr<dsp::EqualizerEngine> eqEngine_;
    std::shared_ptr<dsp::ProEffectsEngine> effectsEngine_;
    std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer_;
    std::shared_ptr<AppVolumeManager> appVolumeManager_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> routingActive_{false};
    std::atomic<uint32_t> sampleRate_{44100};

    std::atomic<TestSignalType> testSignalType_{TestSignalType::None};
    float sinePhase_{0.0f};
    float sweepPhase_{0.0f};
    uint32_t pinkNoiseState_{0x12345678};

    // CoreAudio routing state (taps, aggregate device, IOProc, listeners)
    std::unique_ptr<Impl> impl_;
};

} // namespace eqmax::audio
