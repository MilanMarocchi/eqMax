#include "SpectrumAnalyzer.hpp"
#include <algorithm>
#include <numbers>
#include <iostream>

namespace eqmax::dsp {

SpectrumAnalyzer::SpectrumAnalyzer(size_t fftSize)
    : fftSize_(fftSize) {
    log2n_ = static_cast<size_t>(std::log2(fftSize_));
    ringBuffer_.resize(fftSize_ * 2, 0.0f);
    window_.resize(fftSize_);
    inputBuffer_.resize(fftSize_);
    magnitudeSpectrum_.resize(fftSize_ / 2, 0.0f);
    smoothedBands_.resize(64, 0.0f);

    // Compute Hann Window
    for (size_t i = 0; i < fftSize_; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * i / (fftSize_ - 1)));
    }

#if defined(__APPLE__)
    fftSetup_ = vDSP_create_fftsetup(log2n_, FFT_RADIX2);
    realPart_.resize(fftSize_ / 2, 0.0f);
    imagPart_.resize(fftSize_ / 2, 0.0f);
    splitComplex_.realp = realPart_.data();
    splitComplex_.imagp = imagPart_.data();
#endif
}

SpectrumAnalyzer::~SpectrumAnalyzer() {
#if defined(__APPLE__)
    if (fftSetup_) {
        vDSP_destroy_fftsetup(fftSetup_);
        fftSetup_ = nullptr;
    }
#endif
}

void SpectrumAnalyzer::pushSamples(const float* left, const float* right, size_t numFrames) noexcept {
    if (!left || !right || numFrames == 0) return;

    float pL = 0.0f, pR = 0.0f, sumL = 0.0f, sumR = 0.0f;

    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        for (size_t i = 0; i < numFrames; ++i) {
            float mono = (left[i] + right[i]) * 0.5f;
            ringBuffer_[writePos_] = mono;
            writePos_ = (writePos_ + 1) % ringBuffer_.size();

            float absL = std::abs(left[i]);
            float absR = std::abs(right[i]);
            pL = std::max(pL, absL);
            pR = std::max(pR, absR);
            sumL += left[i] * left[i];
            sumR += right[i] * right[i];
        }
        if (sampleCount_ < ringBuffer_.size()) {
            sampleCount_ += numFrames;
        }
    }

    currentPeakL_.store(pL, std::memory_order_relaxed);
    currentPeakR_.store(pR, std::memory_order_relaxed);
    currentRmsL_.store(std::sqrt(sumL / numFrames), std::memory_order_relaxed);
    currentRmsR_.store(std::sqrt(sumR / numFrames), std::memory_order_relaxed);
}

void SpectrumAnalyzer::processFFT() {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (sampleCount_ < fftSize_) return;

    // Read the most recent fftSize_ samples from ring buffer
    size_t startPos = (writePos_ + ringBuffer_.size() - fftSize_) % ringBuffer_.size();
    for (size_t i = 0; i < fftSize_; ++i) {
        inputBuffer_[i] = ringBuffer_[(startPos + i) % ringBuffer_.size()] * window_[i];
    }

#if defined(__APPLE__)
    if (fftSetup_) {
        // Convert real input into split complex format
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(inputBuffer_.data()), 2, &splitComplex_, 1, fftSize_ / 2);
        // Forward FFT
        vDSP_fft_zrip(fftSetup_, &splitComplex_, 1, log2n_, FFT_FORWARD);

        // Compute magnitude spectrum
        for (size_t i = 0; i < fftSize_ / 2; ++i) {
            float r = splitComplex_.realp[i];
            float im = splitComplex_.imagp[i];
            float mag = std::sqrt(r * r + im * im) / static_cast<float>(fftSize_);
            magnitudeSpectrum_[i] = mag;
        }
    }
#else
    // Fallback magnitude calculation if non-Apple platform
    for (size_t i = 0; i < fftSize_ / 2; ++i) {
        magnitudeSpectrum_[i] = std::abs(inputBuffer_[i]);
    }
#endif
}

std::vector<float> SpectrumAnalyzer::getBandEnergies(size_t numBands) {
    processFFT();

    if (smoothedBands_.size() != numBands) {
        smoothedBands_.assign(numBands, 0.0f);
    }

    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float nyquist = 22050.0f;
    const size_t halfFFT = fftSize_ / 2;

    std::vector<float> currentBands(numBands, 0.0f);

    for (size_t b = 0; b < numBands; ++b) {
        // Logarithmic frequency mapping
        float fStart = minFreq * std::pow(maxFreq / minFreq, static_cast<float>(b) / numBands);
        float fEnd = minFreq * std::pow(maxFreq / minFreq, static_cast<float>(b + 1) / numBands);

        size_t binStart = std::clamp<size_t>(static_cast<size_t>(fStart / nyquist * halfFFT), 0, halfFFT - 1);
        size_t binEnd = std::clamp<size_t>(static_cast<size_t>(fEnd / nyquist * halfFFT), binStart + 1, halfFFT);

        float sum = 0.0f;
        for (size_t k = binStart; k < binEnd; ++k) {
            sum += magnitudeSpectrum_[k];
        }
        float avg = sum / static_cast<float>(binEnd - binStart);

        // Convert magnitude to normalized dB scale [0.0, 1.0]
        float db = 20.0f * std::log10(std::max(avg, 1e-5f)); // -100dB floor
        float norm = std::clamp((db + 70.0f) / 70.0f, 0.0f, 1.0f);
        currentBands[b] = norm;
    }

    // Temporal smoothing & Peak decay (60 FPS visual smoothness)
    const float attack = 0.7f;
    const float decay = 0.88f;

    for (size_t b = 0; b < numBands; ++b) {
        if (currentBands[b] > smoothedBands_[b]) {
            smoothedBands_[b] = smoothedBands_[b] * (1.0f - attack) + currentBands[b] * attack;
        } else {
            smoothedBands_[b] *= decay;
        }
    }

    return smoothedBands_;
}

SpectrumAnalyzer::ChannelLevels SpectrumAnalyzer::getLevels() noexcept {
    return ChannelLevels{
        currentRmsL_.load(std::memory_order_relaxed),
        currentRmsR_.load(std::memory_order_relaxed),
        currentPeakL_.load(std::memory_order_relaxed),
        currentPeakR_.load(std::memory_order_relaxed)
    };
}

} // namespace eqmax::dsp
