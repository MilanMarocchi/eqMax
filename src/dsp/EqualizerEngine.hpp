#pragma once

#include "BiquadFilter.hpp"
#include <vector>
#include <array>
#include <memory>
#include <atomic>
#include <span>

namespace eqmax::dsp {

enum class EQMode {
    ThreeBand,
    TenBand,
    ThirtyTwoBand
};

struct BandInfo {
    float frequency;
    float gaindB;
    FilterType type{FilterType::Peaking};
};

class EqualizerEngine {
public:
    EqualizerEngine();
    ~EqualizerEngine() = default;

    void setMode(EQMode mode, float sampleRate = 44100.0f);
    [[nodiscard]] EQMode mode() const noexcept { return currentMode_; }

    void setBandGain(size_t bandIndex, float gaindB, float sampleRate = 44100.0f);
    void setAllGains(const std::vector<float>& gains, float sampleRate = 44100.0f);
    void resetGains(float sampleRate = 44100.0f);

    [[nodiscard]] size_t numBands() const noexcept;
    [[nodiscard]] const std::vector<BandInfo>& bandInfos() const noexcept { return bandInfos_; }
    [[nodiscard]] float bandGain(size_t bandIndex) const noexcept;

    void setBypass(bool bypassed) noexcept { bypassed_.store(bypassed, std::memory_order_relaxed); }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassed_.load(std::memory_order_relaxed); }

    /**
     * @brief Process stereo non-interleaved channels in place.
     */
    void processStereo(float* left, float* right, size_t numFrames) noexcept;

    /**
     * @brief Process interleaved stereo channels in place.
     */
    void processInterleaved(float* left, float* right, size_t numFrames, float sampleRate) noexcept;

    // ISO standard frequency arrays for 10-band and 32-band EQ
    static const std::array<float, 3> THREE_BAND_FREQS;
    static const std::array<float, 10> TEN_BAND_FREQS;
    static const std::array<float, 32> THIRTY_TWO_BAND_FREQS;

private:
    void rebuildFilters(float sampleRate);

    EQMode currentMode_{EQMode::TenBand};
    std::atomic<bool> bypassed_{false};

    std::vector<BandInfo> bandInfos_;
    // Stereo cascades: separate filter instances for Left and Right channels to maintain phase and state
    std::vector<BiquadFilter> leftFilters_;
    std::vector<BiquadFilter> rightFilters_;
};

} // namespace eqmax::dsp
