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

    std::cout << "--- All DSP Unit Tests Passed Successfully! ---" << std::endl;
    return 0;
}
