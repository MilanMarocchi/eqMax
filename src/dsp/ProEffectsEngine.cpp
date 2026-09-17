#include "ProEffectsEngine.hpp"

namespace eqmax::dsp {

ProEffectsEngine::ProEffectsEngine() {
    setBassBoostdB(0.0f, 44100.0f);
    setTrebleBoostdB(0.0f, 44100.0f);
    haasBufferL_.resize(1024, 0.0f);
}

void ProEffectsEngine::setBassBoostdB(float gaindB, float sampleRate) noexcept {
    bassBoostdB_.store(gaindB, std::memory_order_relaxed);
    leftBassFilter_.configure(FilterType::LowShelf, 90.0f, sampleRate, gaindB, 0.707f);
    rightBassFilter_.configure(FilterType::LowShelf, 90.0f, sampleRate, gaindB, 0.707f);
}

void ProEffectsEngine::setTrebleBoostdB(float gaindB, float sampleRate) noexcept {
    trebleBoostdB_.store(gaindB, std::memory_order_relaxed);
    leftTrebleFilter_.configure(FilterType::HighShelf, 8000.0f, sampleRate, gaindB, 0.707f);
    rightTrebleFilter_.configure(FilterType::HighShelf, 8000.0f, sampleRate, gaindB, 0.707f);
}

void ProEffectsEngine::processStereo(float* left, float* right, size_t numFrames) noexcept {
    if (!left || !right || numFrames == 0) return;

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

    const float attack = 0.001f;
    const float release = 0.999f;

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

        // 4. Soft Clipper / Limiter
        if (enableLimiter) {
            // Peak envelope tracking
            float maxSample = std::max(std::abs(l), std::abs(r));
            if (maxSample > peakEnvelope_) {
                peakEnvelope_ = peakEnvelope_ * attack + maxSample * (1.0f - attack);
            } else {
                peakEnvelope_ = peakEnvelope_ * release + maxSample * (1.0f - release);
            }

            // Smooth saturation curve (tanh soft clipping for pleasant analog warm overdrive)
            l = std::tanh(l);
            r = std::tanh(r);
        }

        left[i] = l;
        right[i] = r;
    }
}

} // namespace eqmax::dsp
