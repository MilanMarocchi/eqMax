#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include "BiquadFilter.hpp"
#include "EqualizerEngine.hpp"
#include "ProEffectsEngine.hpp"
#include "SpectrumAnalyzer.hpp"

int main() {
    std::cout << "--- Running eqMax DSP Engine Unit Tests ---" << std::endl;

    // Test 1: Biquad Filter Initialization and Processing
    eqmax::dsp::BiquadFilter filter;
    filter.configure(eqmax::dsp::FilterType::Peaking, 1000.0f, 44100.0f, 6.0f, 1.414f);
    
    float inputSample = 0.5f;
    float outputSample = filter.processSample(inputSample);
    std::cout << "[Test 1] Biquad processSample (1kHz Peak +6dB): Input 0.5 -> Output " << outputSample << std::endl;
    assert(!std::isnan(outputSample) && !std::isinf(outputSample));

    // Test 2: EqualizerEngine (10-Band Mode)
    eqmax::dsp::EqualizerEngine eq;
    eq.setMode(eqmax::dsp::EQMode::TenBand);
    eq.setBandGain(4, 3.0f); // 500Hz band set to +3dB

    std::vector<float> leftBuffer(512, 0.1f);
    std::vector<float> rightBuffer(512, 0.1f);
    eq.processInterleaved(leftBuffer.data(), rightBuffer.data(), 512, 44100.0f);

    std::cout << "[Test 2] EqualizerEngine processed 512 stereo samples cleanly." << std::endl;

    // Test 3: ProEffectsEngine Volume Boost & Limiter
    eqmax::dsp::ProEffectsEngine effects;
    effects.setVolumeBoostdB(12.0f); // +12 dB boost
    effects.setLimiterEnabled(true);
    effects.setSpatialWidth(0.5f);
    effects.setBalance(0.0f); // Center

    std::vector<float> testBlockL(256, 0.5f);
    std::vector<float> testBlockR(256, 0.5f);
    effects.processStereo(testBlockL.data(), testBlockR.data(), 256);

    std::cout << "[Test 3] ProEffectsEngine processed 256 samples with +12dB boost. Sample 0 out: " 
              << testBlockL[0] << " (clipping prevented by limiter)" << std::endl;
    assert(std::abs(testBlockL[0]) <= 1.05f); // Limiter prevents hard clipping above ~1.0

    // Test 4: SpectrumAnalyzer FFT processing
    eqmax::dsp::SpectrumAnalyzer spectrum(512);
    spectrum.pushSamples(testBlockL.data(), testBlockR.data(), 256);
    auto bands = spectrum.getBandEnergies(32);
    std::cout << "[Test 4] SpectrumAnalyzer computed " << bands.size() << " frequency band energies." << std::endl;
    assert(bands.size() == 32);

    // The limiter must be transparent below its ceiling, with no tanh coloration at unity.
    eqmax::dsp::ProEffectsEngine transparent;
    std::vector<float> cleanL{0.1f, -0.2f, 0.5f, -0.75f};
    auto cleanR = cleanL;
    const auto original = cleanL;
    transparent.processStereo(cleanL.data(), cleanR.data(), cleanL.size());
    for (size_t i = 0; i < cleanL.size(); ++i) assert(std::abs(cleanL[i] - original[i]) < 1e-6f);
    transparent.setVolumeBoostdB(24);
    transparent.setBassBoostdB(12);
    transparent.setSpatialWidth(2);
    transparent.setBypass(true);
    cleanL = original; cleanR = original;
    transparent.processStereo(cleanL.data(), cleanR.data(), cleanL.size());
    assert(cleanL == original && cleanR == original);
    transparent.setBypass(false);
    cleanL.assign(512, 0.8f); cleanR.assign(512, 0.4f);
    transparent.processStereo(cleanL.data(), cleanR.data(), cleanL.size());
    for (size_t i = 0; i < cleanL.size(); ++i) {
        assert(std::isfinite(cleanL[i]) && std::abs(cleanL[i]) <= 0.981f);
        assert(std::isfinite(cleanR[i]) && std::abs(cleanR[i]) <= 0.981f);
    }

    // The displayed response uses the same biquad transfer function as the audio path.
    eqmax::dsp::BiquadFilter response;
    response.configure(eqmax::dsp::FilterType::Peaking, 1000, 48000, 6, 1.414f);
    assert(std::abs(response.magnitudedB(1000) - 6) < 0.01f);
    assert(std::abs(response.magnitudedB(20)) < 0.01f);
    for (float rate : {44100.0f, 48000.0f, 96000.0f}) {
        eqmax::dsp::EqualizerEngine accurate;
        accurate.setSampleRate(rate);
        // UI edits must keep the actual device's sample rate rather than reverting to 44.1k.
        accurate.setMode(eqmax::dsp::EQMode::TenBand);
        accurate.setBandGain(5, 6);
        const size_t frames = static_cast<size_t>(rate);
        std::vector<float> signalL(frames), signalR(frames);
        double inputEnergy = 0, outputEnergy = 0;
        for (size_t i = 0; i < frames; ++i) {
            signalL[i] = signalR[i] = 0.1f * std::sin(2 * std::numbers::pi * 1000 * i / rate);
            if (i > frames / 2) inputEnergy += signalL[i] * signalL[i];
        }
        accurate.processStereo(signalL.data(), signalR.data(), frames);
        for (size_t i = frames / 2 + 1; i < frames; ++i) outputEnergy += signalL[i] * signalL[i];
        assert(std::abs(10 * std::log10(outputEnergy / inputEnergy) - 6) < 0.05);
        accurate.setBypass(true);
        const auto bypassInput = signalL;
        accurate.processStereo(signalL.data(), signalR.data(), frames);
        assert(signalL == bypassInput);
    }
    std::cout << "[Tests 5–8] Transparent limiter, complete effects bypass, response, and device sample rates passed." << std::endl;

    std::cout << "--- All DSP Unit Tests Passed Successfully! ---" << std::endl;
    return 0;
}
