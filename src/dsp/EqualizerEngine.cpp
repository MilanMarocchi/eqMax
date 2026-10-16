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
    bandInfos_.reserve(THIRTY_TWO_BAND_FREQS.size());
    leftFilters_.resize(THIRTY_TWO_BAND_FREQS.size());
    rightFilters_.resize(THIRTY_TWO_BAND_FREQS.size());
    setMode(EQMode::TenBand, 44100.0f);
}

void EqualizerEngine::setMode(EQMode mode, float sampleRate) {
    setSampleRate(sampleRate);
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

    publishConfiguration();
}

void EqualizerEngine::setSampleRate(float sampleRate) noexcept {
    if (std::isfinite(sampleRate) && sampleRate >= 8000) sampleRate_.store(sampleRate, std::memory_order_relaxed);
}

void EqualizerEngine::publishConfiguration() {
    std::lock_guard lock(configurationMutex_);
    std::copy(bandInfos_.begin(), bandInfos_.end(), pending_.bands.begin());
    pending_.count = bandInfos_.size();
    pending_.mode = currentMode_;
    ++pending_.version;
}

void EqualizerEngine::updateAudioConfiguration() noexcept {
    const auto previousVersion = audio_.version;
    const auto previousMode = audio_.mode;
    // Never wait for the UI on the real-time thread; a pending change can wait one block.
    if (configurationMutex_.try_lock()) {
        audio_ = pending_;
        configurationMutex_.unlock();
    }
    const float rate = sampleRate_.load(std::memory_order_relaxed);
    if (audio_.version == previousVersion && rate == audioSampleRate_) return;
    const bool reset = previousVersion == 0 || previousMode != audio_.mode || rate != audioSampleRate_;
    const float q = audio_.mode == EQMode::ThreeBand ? 0.707f : audio_.mode == EQMode::TenBand ? 1.414f : 4.318f;
    for (size_t i = 0; i < audio_.count; ++i) {
        const auto& band = audio_.bands[i];
        leftFilters_[i].configure(band.type, band.frequency, rate, band.gaindB, q);
        rightFilters_[i].configure(band.type, band.frequency, rate, band.gaindB, q);
        if (reset) { leftFilters_[i].reset(); rightFilters_[i].reset(); }
    }
    audioSampleRate_ = rate;
}

void EqualizerEngine::setBandGain(size_t bandIndex, float gaindB, float sampleRate) {
    if (bandIndex >= bandInfos_.size() || !std::isfinite(gaindB)) return;
    setSampleRate(sampleRate);
    bandInfos_[bandIndex].gaindB = std::clamp(gaindB, -12.0f, 12.0f);
    publishConfiguration();
}

void EqualizerEngine::setAllGains(const std::vector<float>& gains, float sampleRate) {
    setSampleRate(sampleRate);
    const size_t limit = std::min(gains.size(), bandInfos_.size());
    for (size_t i = 0; i < limit; ++i) {
        if (std::isfinite(gains[i])) bandInfos_[i].gaindB = std::clamp(gains[i], -12.0f, 12.0f);
    }
    publishConfiguration();
}

void EqualizerEngine::resetGains(float sampleRate) {
    setSampleRate(sampleRate);
    for (auto& band : bandInfos_) band.gaindB = 0;
    publishConfiguration();
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

    updateAudioConfiguration();
    const size_t filterCount = audio_.count;
    for (size_t f = 0; f < filterCount; ++f) {
        // Skip filters with 0.0dB gain for CPU optimization
        if (std::abs(audio_.bands[f].gaindB) < 0.01f) continue;
        for (size_t i = 0; i < numFrames; ++i) {
            left[i] = leftFilters_[f].processSample(left[i]);
            right[i] = rightFilters_[f].processSample(right[i]);
        }
    }
}

void EqualizerEngine::processInterleaved(float* left, float* right, size_t numFrames, float sampleRate) noexcept {
    setSampleRate(sampleRate);
    processStereo(left, right, numFrames);
}

} // namespace eqmax::dsp
