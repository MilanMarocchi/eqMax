#pragma once

#include <cmath>
#include <numbers>
#include <algorithm>
#include <array>
#include <span>

namespace eqmax::dsp {

enum class FilterType {
    Peaking,
    LowShelf,
    HighShelf,
    LowPass,
    HighPass,
    BandPass
};

/**
 * @brief Second-order IIR Biquad Filter implementing Transposed Direct Form II.
 * Designed for low latency, high numerical stability, and zero heap allocation in process loops.
 */
class BiquadFilter {
public:
    BiquadFilter() noexcept = default;

    /**
     * @brief Configure filter parameters and recalculate coefficients using Robert Bristow-Johnson formulas.
     * 
     * @param type Filter topology
     * @param centerFreq Center or cutoff frequency in Hz
     * @param sampleRate Audio sample rate in Hz (e.g. 44100, 48000, 96000)
     * @param gaindB Gain in decibels for peaking and shelf filters (-24dB to +24dB)
     * @param Q Quality factor (bandwidth indicator, default ~1.414 for 1 octave)
     */
    void configure(FilterType type, float centerFreq, float sampleRate, float gaindB, float Q = 1.41421356f) noexcept;

    /**
     * @brief Reset delay registers to zero (prevents click/pop on startup).
     */
    void reset() noexcept;

    /**
     * @brief Process a single sample through Transposed Direct Form II.
     */
    [[nodiscard]] inline float processSample(float input) noexcept {
        float output = b0_ * input + z1_;
        z1_ = b1_ * input - a1_ * output + z2_;
        z2_ = b2_ * input - a2_ * output;
        return output;
    }

    /**
     * @brief Process a block of samples in place.
     */
    void processBlock(std::span<float> samples) noexcept;

    [[nodiscard]] float centerFrequency() const noexcept { return centerFreq_; }
    [[nodiscard]] float gaindB() const noexcept { return gaindB_; }
    [[nodiscard]] FilterType type() const noexcept { return type_; }
    [[nodiscard]] float magnitudedB(float frequency) const noexcept;

private:
    FilterType type_{FilterType::Peaking};
    float centerFreq_{1000.0f};
    float sampleRate_{44100.0f};
    float gaindB_{0.0f};
    float Q_{1.41421356f};

    // Coefficients normalized by a0
    float b0_{1.0f};
    float b1_{0.0f};
    float b2_{0.0f};
    float a1_{0.0f};
    float a2_{0.0f};

    // State delay registers for Transposed Direct Form II
    float z1_{0.0f};
    float z2_{0.0f};
};

} // namespace eqmax::dsp
