#include "BiquadFilter.hpp"
#include <complex>

namespace eqmax::dsp {

void BiquadFilter::configure(FilterType type, float centerFreq, float sampleRate, float gaindB, float Q) noexcept {
    type_ = type;
    centerFreq_ = std::clamp(centerFreq, 20.0f, sampleRate * 0.49f);
    sampleRate_ = std::max(8000.0f, sampleRate);
    gaindB_ = std::clamp(gaindB, -24.0f, 24.0f);
    Q_ = std::max(0.1f, Q);

    const float omega = 2.0f * std::numbers::pi_v<float> * centerFreq_ / sampleRate_;
    const float sinw = std::sin(omega);
    const float cosw = std::cos(omega);
    const float A = std::pow(10.0f, gaindB_ / 40.0f);
    const float alpha = sinw / (2.0f * Q_);

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

    switch (type_) {
        case FilterType::Peaking: {
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosw;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosw;
            a2 = 1.0f - alpha / A;
            break;
        }
        case FilterType::LowShelf: {
            const float beta = std::sqrt(A) / Q_;
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw + beta * sinw);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw - beta * sinw);
            a0 = (A + 1.0f) + (A - 1.0f) * cosw + beta * sinw;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw);
            a2 = (A + 1.0f) + (A - 1.0f) * cosw - beta * sinw;
            break;
        }
        case FilterType::HighShelf: {
            const float beta = std::sqrt(A) / Q_;
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw + beta * sinw);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw - beta * sinw);
            a0 = (A + 1.0f) - (A - 1.0f) * cosw + beta * sinw;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw);
            a2 = (A + 1.0f) - (A - 1.0f) * cosw - beta * sinw;
            break;
        }
        case FilterType::LowPass: {
            b0 = (1.0f - cosw) / 2.0f;
            b1 = 1.0f - cosw;
            b2 = (1.0f - cosw) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw;
            a2 = 1.0f - alpha;
            break;
        }
        case FilterType::HighPass: {
            b0 = (1.0f + cosw) / 2.0f;
            b1 = -(1.0f + cosw);
            b2 = (1.0f + cosw) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw;
            a2 = 1.0f - alpha;
            break;
        }
        case FilterType::BandPass: {
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw;
            a2 = 1.0f - alpha;
            break;
        }
    }

    const float invA0 = 1.0f / a0;
    b0_ = b0 * invA0;
    b1_ = b1 * invA0;
    b2_ = b2 * invA0;
    a1_ = a1 * invA0;
    a2_ = a2 * invA0;
}

void BiquadFilter::reset() noexcept {
    z1_ = 0.0f;
    z2_ = 0.0f;
}

float BiquadFilter::magnitudedB(float frequency) const noexcept {
    const auto z = std::polar(1.0, -2.0 * std::numbers::pi * frequency / sampleRate_);
    const auto numerator = static_cast<double>(b0_) + static_cast<double>(b1_) * z + static_cast<double>(b2_) * z * z;
    const auto denominator = 1.0 + static_cast<double>(a1_) * z + static_cast<double>(a2_) * z * z;
    return static_cast<float>(20.0 * std::log10(std::max(1e-12, std::abs(numerator / denominator))));
}

void BiquadFilter::processBlock(std::span<float> samples) noexcept {
    for (float& sample : samples) {
        sample = processSample(sample);
    }
}

} // namespace eqmax::dsp
