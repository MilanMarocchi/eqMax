#pragma once

#include "BiquadFilter.hpp"
#include <vector>
#include <array>
#include <memory>
#include <atomic>
#include <span>
#include <mutex>

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

    void setMode(EQMode mode, float sampleRate = 0.0f);
    [[nodiscard]] EQMode mode() const noexcept { return currentMode_; }

    void setBandGain(size_t bandIndex, float gaindB, float sampleRate = 0.0f);
    void setAllGains(const std::vector<float>& gains, float sampleRate = 0.0f);
    void resetGains(float sampleRate = 0.0f);
    void setSampleRate(float sampleRate) noexcept;

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
    void publishConfiguration();
    void updateAudioConfiguration() noexcept;

    EQMode currentMode_{EQMode::TenBand};
    std::atomic<bool> bypassed_{false};

    // bandInfos_ belongs to the UI. The audio thread only reads its own fixed-size snapshot.
    std::vector<BandInfo> bandInfos_;
    struct Configuration {
        std::array<BandInfo, 32> bands{};
        size_t count{0};
        EQMode mode{EQMode::TenBand};
        uint64_t version{0};
    };
    std::mutex configurationMutex_;
    Configuration pending_, audio_;
    std::atomic<float> sampleRate_{44100};
    float audioSampleRate_{0};
    // Stereo cascades: separate filter instances for Left and Right channels to maintain phase and state
    std::vector<BiquadFilter> leftFilters_;
    std::vector<BiquadFilter> rightFilters_;
};

} // namespace eqmax::dsp
