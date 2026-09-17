#pragma once

#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>
#include <span>

#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#endif

namespace eqmax::dsp {

class SpectrumAnalyzer {
public:
    explicit SpectrumAnalyzer(size_t fftSize = 1024);
    ~SpectrumAnalyzer();

    // Disable copy for thread safety and pointer resources
    SpectrumAnalyzer(const SpectrumAnalyzer&) = delete;
    SpectrumAnalyzer& operator=(const SpectrumAnalyzer&) = delete;

    /**
     * @brief Push incoming audio frames into the ring buffer for FFT processing.
     */
    void pushSamples(const float* left, const float* right, size_t numFrames) noexcept;

    /**
     * @brief Execute FFT analysis and compute log-scaled energy spectrum bins.
     * 
     * @param numBands Number of output spectrum display bars (e.g. 32 or 64)
     * @return std::vector<float> Normalized energy levels [0.0, 1.0] per band
     */
    std::vector<float> getBandEnergies(size_t numBands = 32);

    /**
     * @brief Get instantaneous RMS and Peak levels for L/R channel VU meters.
     */
    struct ChannelLevels {
        float rmsLeft{0.0f};
        float rmsRight{0.0f};
        float peakLeft{0.0f};
        float peakRight{0.0f};
    };
    ChannelLevels getLevels() noexcept;

private:
    void processFFT();

    size_t fftSize_{1024};
    size_t log2n_{10};

    // Lock-free ring buffer for audio samples
    std::vector<float> ringBuffer_;
    size_t writePos_{0};
    size_t sampleCount_{0};
    std::mutex bufferMutex_;

    // Windowing & FFT work buffers
    std::vector<float> window_;
    std::vector<float> inputBuffer_;
    std::vector<float> magnitudeSpectrum_;
    std::vector<float> smoothedBands_;

    // VU Meter levels tracking
    std::atomic<float> currentPeakL_{0.0f};
    std::atomic<float> currentPeakR_{0.0f};
    std::atomic<float> currentRmsL_{0.0f};
    std::atomic<float> currentRmsR_{0.0f};

#if defined(__APPLE__)
    FFTSetup fftSetup_{nullptr};
    DSPSplitComplex splitComplex_{nullptr, nullptr};
    std::vector<float> realPart_;
    std::vector<float> imagPart_;
#endif
};

} // namespace eqmax::dsp
