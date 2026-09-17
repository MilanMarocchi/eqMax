#include "EqualizerEngine.hpp"

namespace eqmax::dsp {

const std::array<float, 3> EqualizerEngine::THREE_BAND_FREQS = { 100.0f, 1000.0f, 10000.0f };

const std::array<float, 10> EqualizerEngine::TEN_BAND_FREQS = {
    32.0f, 64.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

const std::array<float, 32> EqualizerEngine::THIRTY_TWO_BAND_FREQS = {
    20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f,
    200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f,
    2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f,
    18000.0f, 20000.0f
};

EqualizerEngine::EqualizerEngine() {
    setMode(EQMode::TenBand, 44100.0f);
}

void EqualizerEngine::setMode(EQMode mode, float sampleRate) {
    currentMode_ = mode;
    bandInfos_.clear();

    if (currentMode_ == EQMode::ThreeBand) {
        bandInfos_.push_back({ THREE_BAND_FREQS[0], 0.0f, FilterType::LowShelf });
        bandInfos_.push_back({ THREE_BAND_FREQS[1], 0.0f, FilterType::Peaking });
        bandInfos_.push_back({ THREE_BAND_FREQS[2], 0.0f, FilterType::HighShelf });
    } else if (currentMode_ == EQMode::TenBand) {
        for (size_t i = 0; i < TEN_BAND_FREQS.size(); ++i) {
            FilterType type = FilterType::Peaking;
            if (i == 0) type = FilterType::LowShelf;
            else if (i == TEN_BAND_FREQS.size() - 1) type = FilterType::HighShelf;
            bandInfos_.push_back({ TEN_BAND_FREQS[i], 0.0f, type });
        }
    } else { // ThirtyTwoBand
        for (size_t i = 0; i < THIRTY_TWO_BAND_FREQS.size(); ++i) {
            FilterType type = FilterType::Peaking;
            if (i == 0) type = FilterType::LowShelf;
            else if (i == THIRTY_TWO_BAND_FREQS.size() - 1) type = FilterType::HighShelf;
            bandInfos_.push_back({ THIRTY_TWO_BAND_FREQS[i], 0.0f, type });
        }
    }

    rebuildFilters(sampleRate);
}

void EqualizerEngine::rebuildFilters(float sampleRate) {
    const size_t count = bandInfos_.size();
    leftFilters_.resize(count);
    rightFilters_.resize(count);

    float qFactor = 1.414f;
    if (currentMode_ == EQMode::ThirtyTwoBand) {
        qFactor = 4.318f; // Narrow bandwidth (~1/3 octave) for 32 bands
    } else if (currentMode_ == EQMode::TenBand) {
        qFactor = 1.414f; // ~1 octave for 10 bands
    } else {
        qFactor = 0.707f; // Wide shelf for 3 bands
    }

    for (size_t i = 0; i < count; ++i) {
        leftFilters_[i].configure(bandInfos_[i].type, bandInfos_[i].frequency, sampleRate, bandInfos_[i].gaindB, qFactor);
        rightFilters_[i].configure(bandInfos_[i].type, bandInfos_[i].frequency, sampleRate, bandInfos_[i].gaindB, qFactor);
    }
}

void EqualizerEngine::setBandGain(size_t bandIndex, float gaindB, float sampleRate) {
    if (bandIndex >= bandInfos_.size()) return;
    bandInfos_[bandIndex].gaindB = gaindB;

    float qFactor = 1.414f;
    if (currentMode_ == EQMode::ThirtyTwoBand) qFactor = 4.318f;
    else if (currentMode_ == EQMode::ThreeBand) qFactor = 0.707f;

    leftFilters_[bandIndex].configure(bandInfos_[bandIndex].type, bandInfos_[bandIndex].frequency, sampleRate, gaindB, qFactor);
    rightFilters_[bandIndex].configure(bandInfos_[bandIndex].type, bandInfos_[bandIndex].frequency, sampleRate, gaindB, qFactor);
}

void EqualizerEngine::setAllGains(const std::vector<float>& gains, float sampleRate) {
    const size_t limit = std::min(gains.size(), bandInfos_.size());
    for (size_t i = 0; i < limit; ++i) {
        setBandGain(i, gains[i], sampleRate);
    }
}

void EqualizerEngine::resetGains(float sampleRate) {
    for (size_t i = 0; i < bandInfos_.size(); ++i) {
        setBandGain(i, 0.0f, sampleRate);
    }
}

size_t EqualizerEngine::numBands() const noexcept {
    return bandInfos_.size();
}

float EqualizerEngine::bandGain(size_t bandIndex) const noexcept {
    if (bandIndex < bandInfos_.size()) {
        return bandInfos_[bandIndex].gaindB;
    }
    return 0.0f;
}

void EqualizerEngine::processStereo(float* left, float* right, size_t numFrames) noexcept {
    if (bypassed_.load(std::memory_order_relaxed) || !left || !right) return;

    const size_t filterCount = leftFilters_.size();
    for (size_t f = 0; f < filterCount; ++f) {
        // Skip filters with 0.0dB gain for CPU optimization
        if (std::abs(bandInfos_[f].gaindB) < 0.01f) continue;
        for (size_t i = 0; i < numFrames; ++i) {
            left[i] = leftFilters_[f].processSample(left[i]);
            right[i] = rightFilters_[f].processSample(right[i]);
        }
    }
}

void EqualizerEngine::processInterleaved(float* left, float* right, size_t numFrames, float sampleRate) noexcept {
    processStereo(left, right, numFrames);
}

} // namespace eqmax::dsp
