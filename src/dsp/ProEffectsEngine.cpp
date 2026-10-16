#include "ProEffectsEngine.hpp"

namespace eqmax::dsp {

ProEffectsEngine::ProEffectsEngine() {
    setBassBoostdB(0.0f, 44100.0f);
    setTrebleBoostdB(0.0f, 44100.0f);
    haasBufferL_.resize(1024, 0.0f);
}

void ProEffectsEngine::setSampleRate(float sampleRate) noexcept {
    if (std::isfinite(sampleRate) && sampleRate >= 8000) sampleRate_.store(sampleRate, std::memory_order_relaxed);
}

void ProEffectsEngine::setBassBoostdB(float gaindB, float sampleRate) noexcept {
    setSampleRate(sampleRate);
    if (std::isfinite(gaindB)) bassBoostdB_.store(std::clamp(gaindB, 0.0f, 12.0f), std::memory_order_relaxed);
}

void ProEffectsEngine::setTrebleBoostdB(float gaindB, float sampleRate) noexcept {
    setSampleRate(sampleRate);
    if (std::isfinite(gaindB)) trebleBoostdB_.store(std::clamp(gaindB, 0.0f, 12.0f), std::memory_order_relaxed);
}

void ProEffectsEngine::processStereo(float* left, float* right, size_t numFrames) noexcept {
    if (isBypassed() || !left || !right || numFrames == 0) return;

    const float boostdB = volumeBoostdB_.load(std::memory_order_relaxed);
    const float boostLinear = std::pow(10.0f, boostdB / 20.0f);

    const float bal = balance_.load(std::memory_order_relaxed);
    // Constant power panning law
    const float angle = (bal + 1.0f) * (std::numbers::pi_v<float> / 4.0f);
    const float gainL = std::cos(angle) * std::numbers::sqrt2_v<float> * boostLinear;
    const float gainR = std::sin(angle) * std::numbers::sqrt2_v<float> * boostLinear;

    const float width = spatialWidth_.load(std::memory_order_relaxed);
    const bool enableLimiter = limiterEnabled_.load(std::memory_order_relaxed);

    const float bassdB = bassBoostdB_.load(std::memory_order_relaxed);
    const float trebledB = trebleBoostdB_.load(std::memory_order_relaxed);

    const float sampleRate = sampleRate_.load(std::memory_order_relaxed);
    // Coefficients and filter state belong exclusively to the audio thread.
    if (sampleRate != audioSampleRate_ || bassdB != audioBassdB_) {
        leftBassFilter_.configure(FilterType::LowShelf, 90, sampleRate, bassdB, 0.707f);
        rightBassFilter_.configure(FilterType::LowShelf, 90, sampleRate, bassdB, 0.707f);
    }
    if (sampleRate != audioSampleRate_ || trebledB != audioTrebledB_) {
        leftTrebleFilter_.configure(FilterType::HighShelf, 8000, sampleRate, trebledB, 0.707f);
        rightTrebleFilter_.configure(FilterType::HighShelf, 8000, sampleRate, trebledB, 0.707f);
    }
    audioSampleRate_ = sampleRate;
    audioBassdB_ = bassdB;
    audioTrebledB_ = trebledB;

    const float release = std::exp(-1.0f / (0.05f * sampleRate));

    for (size_t i = 0; i < numFrames; ++i) {
        float l = left[i];
        float r = right[i];

        // 1. Bass & Treble Sub-Enhancer
        if (std::abs(bassdB) > 0.01f) {
            l = leftBassFilter_.processSample(l);
            r = rightBassFilter_.processSample(r);
        }
        if (std::abs(trebledB) > 0.01f) {
            l = leftTrebleFilter_.processSample(l);
            r = rightTrebleFilter_.processSample(r);
        }

        // 2. Spatial 3D Surround Widener (Mid-Side Processing)
        if (std::abs(width - 1.0f) > 0.01f) {
            float mid = (l + r) * 0.5f;
            float side = (l - r) * 0.5f * width;
            l = mid + side;
            r = mid - side;
        }

        // 3. Apply Gain & Balance
        l *= gainL;
        r *= gainR;

        // 4. Stereo-linked peak limiter. Unity audio stays untouched below the ceiling.
        if (enableLimiter) {
            const float peak = std::max(std::abs(l), std::abs(r));
            peakEnvelope_ = std::max(peak, peakEnvelope_ * release);
            const float gain = peakEnvelope_ > 0.98f ? 0.98f / peakEnvelope_ : 1.0f;
            l *= gain;
            r *= gain;
        } else {
            peakEnvelope_ = 0;
        }

        left[i] = l;
        right[i] = r;
    }
}

} // namespace eqmax::dsp
