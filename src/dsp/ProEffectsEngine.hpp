#pragma once

#include "BiquadFilter.hpp"
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>

namespace eqmax::dsp {

class ProEffectsEngine {
public:
    ProEffectsEngine();
    ~ProEffectsEngine() = default;

    // Volume Boost (Gain in dB: 0dB to +24dB)
    void setVolumeBoostdB(float gaindB) noexcept { volumeBoostdB_.store(gaindB, std::memory_order_relaxed); }
    [[nodiscard]] float volumeBoostdB() const noexcept { return volumeBoostdB_.load(std::memory_order_relaxed); }

    // Limiter toggle
    void setLimiterEnabled(bool enabled) noexcept { limiterEnabled_.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] bool isLimiterEnabled() const noexcept { return limiterEnabled_.load(std::memory_order_relaxed); }

    // Balance control (-1.0 = full left, 0.0 = center, +1.0 = full right)
    void setBalance(float balance) noexcept { balance_.store(std::clamp(balance, -1.0f, 1.0f), std::memory_order_relaxed); }
    [[nodiscard]] float balance() const noexcept { return balance_.load(std::memory_order_relaxed); }

    // Spatial Surround Width (0.0 = Mono, 1.0 = Normal Stereo, 2.0 = 3D Spatial Width)
    void setSpatialWidth(float width) noexcept { spatialWidth_.store(std::clamp(width, 0.0f, 2.5f), std::memory_order_relaxed); }
    [[nodiscard]] float spatialWidth() const noexcept { return spatialWidth_.load(std::memory_order_relaxed); }

    // Bass Boost Intensity (0.0 to 12.0 dB)
    void setBassBoostdB(float gaindB, float sampleRate = 44100.0f) noexcept;
    [[nodiscard]] float bassBoostdB() const noexcept { return bassBoostdB_.load(std::memory_order_relaxed); }

    // Treble Boost Intensity (0.0 to 12.0 dB)
    void setTrebleBoostdB(float gaindB, float sampleRate = 44100.0f) noexcept;
    [[nodiscard]] float trebleBoostdB() const noexcept { return trebleBoostdB_.load(std::memory_order_relaxed); }

    /**
     * @brief Process stereo audio buffer in place applying all enabled pro sound enhancements.
     */
    void processStereo(float* left, float* right, size_t numFrames) noexcept;

private:
    std::atomic<float> volumeBoostdB_{0.0f};
    std::atomic<bool> limiterEnabled_{true};
    std::atomic<float> balance_{0.0f};
    std::atomic<float> spatialWidth_{1.0f}; // 1.0 = unchanged stereo
    std::atomic<float> bassBoostdB_{0.0f};
    std::atomic<float> trebleBoostdB_{0.0f};

    // Dedicated filters for Bass/Treble boost sub-engine
    BiquadFilter leftBassFilter_;
    BiquadFilter rightBassFilter_;
    BiquadFilter leftTrebleFilter_;
    BiquadFilter rightTrebleFilter_;

    // Limiter envelope state
    float peakEnvelope_{0.0f};

    // Delay buffer for Haas effect spatial widening
    std::vector<float> haasBufferL_;
};

} // namespace eqmax::dsp
