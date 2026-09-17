#include "AudioDeviceManager.hpp"
#include <iostream>
#include <cmath>
#include <numbers>
#include <random>

#if defined(__APPLE__)
#import <CoreAudio/CoreAudio.h>
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>
#endif

namespace eqmax::audio {

#if defined(__APPLE__)
static OSStatus CoreAudioIOCallback(
    AudioObjectID inDevice,
    const AudioTimeStamp* inNow,
    const AudioBufferList* inInputData,
    const AudioTimeStamp* inInputTime,
    AudioBufferList* outOutputData,
    const AudioTimeStamp* inOutputTime,
    void* inClientData) {

    auto* manager = static_cast<AudioDeviceManager*>(inClientData);
    if (!manager) return noErr;

    const AudioBufferList* srcBufList = (inInputData && inInputData->mNumberBuffers > 0 && inInputData->mBuffers[0].mData) 
                                        ? inInputData : outOutputData;
    if (!srcBufList || srcBufList->mNumberBuffers == 0) return noErr;

    for (UInt32 i = 0; i < srcBufList->mNumberBuffers; ++i) {
        const AudioBuffer& buf = srcBufList->mBuffers[i];
        if (buf.mNumberChannels >= 2 && buf.mData) {
            auto* samples = static_cast<float*>(buf.mData);
            UInt32 numFrames = buf.mDataByteSize / (sizeof(float) * buf.mNumberChannels);

            std::vector<float> leftBuffer(numFrames);
            std::vector<float> rightBuffer(numFrames);

            for (UInt32 f = 0; f < numFrames; ++f) {
                leftBuffer[f] = samples[f * 2];
                rightBuffer[f] = samples[f * 2 + 1];
            }

            manager->processAudioBlock(leftBuffer.data(), rightBuffer.data(), numFrames);

            if (outOutputData && i < outOutputData->mNumberBuffers && outOutputData->mBuffers[i].mData) {
                auto* outSamples = static_cast<float*>(outOutputData->mBuffers[i].mData);
                for (UInt32 f = 0; f < numFrames; ++f) {
                    outSamples[f * 2] = leftBuffer[f];
                    outSamples[f * 2 + 1] = rightBuffer[f];
                }
            }
        }
    }
    return noErr;
}
#endif

AudioDeviceManager::AudioDeviceManager(
    std::shared_ptr<dsp::EqualizerEngine> eqEngine,
    std::shared_ptr<dsp::ProEffectsEngine> effectsEngine,
    std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer)
    : eqEngine_(std::move(eqEngine)),
      effectsEngine_(std::move(effectsEngine)),
      spectrumAnalyzer_(std::move(spectrumAnalyzer)) {
}

AudioDeviceManager::~AudioDeviceManager() {
    stop();
}

std::vector<AudioDeviceInfo> AudioDeviceManager::getAudioDevices() {
    std::vector<AudioDeviceInfo> devices;

#if defined(__APPLE__)
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 dataSize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &dataSize);
    if (status != noErr || dataSize == 0) {
        devices.push_back({ 1, "default_speaker", "System Output (Built-in Speakers)", false, true, 44100 });
        devices.push_back({ 2, "default_headphone", "External Headphones", false, true, 48000 });
        return devices;
    }

    UInt32 deviceCount = dataSize / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> audioDevices(deviceCount);
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &dataSize, audioDevices.data());

    for (UInt32 i = 0; i < deviceCount; ++i) {
        AudioDeviceID devId = audioDevices[i];
        AudioDeviceInfo info;
        info.id = devId;

        // Get device name
        CFStringRef deviceName = nullptr;
        dataSize = sizeof(CFStringRef);
        propertyAddress.mSelector = kAudioObjectPropertyName;
        propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        if (AudioObjectGetPropertyData(devId, &propertyAddress, 0, nullptr, &dataSize, &deviceName) == noErr && deviceName) {
            char nameBuf[256];
            if (CFStringGetCString(deviceName, nameBuf, sizeof(nameBuf), kCFStringEncodingUTF8)) {
                info.name = nameBuf;
            }
            CFRelease(deviceName);
        }

        // Check output capability
        propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
        propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
        if (AudioObjectGetPropertyDataSize(devId, &propertyAddress, 0, nullptr, &dataSize) == noErr && dataSize > 0) {
            info.isOutput = true;
        }

        // Check input capability
        propertyAddress.mScope = kAudioDevicePropertyScopeInput;
        if (AudioObjectGetPropertyDataSize(devId, &propertyAddress, 0, nullptr, &dataSize) == noErr && dataSize > 0) {
            info.isInput = true;
        }

        if (info.name.empty()) {
            info.name = "Audio Device #" + std::to_string(devId);
        }

        devices.push_back(info);
    }
#else
    devices.push_back({ 1, "default_speaker", "System Speaker", false, true, 44100 });
#endif

    if (devices.empty()) {
        devices.push_back({ 1, "default_output", "CoreAudio Output Device", false, true, 44100 });
    }

    return devices;
}

bool AudioDeviceManager::setOutputDevice(uint32_t deviceId) {
    if (outputDeviceId_ == deviceId && isRunning_.load()) return true;

    bool wasRunning = isRunning_.load();
    if (wasRunning) stop();

    outputDeviceId_ = deviceId;
    std::cout << "[AudioDeviceManager] Selected Output Device ID: " << deviceId << std::endl;

    if (wasRunning) start();
    return true;
}

bool AudioDeviceManager::setInputDevice(uint32_t deviceId) {
    inputDeviceId_ = deviceId;
    std::cout << "[AudioDeviceManager] Selected Input Device ID: " << deviceId << std::endl;
    return true;
}

bool AudioDeviceManager::start() {
    if (isRunning_.load()) return true;

#if defined(__APPLE__)
    if (outputDeviceId_ == 0) {
        AudioDeviceID defaultOutput = 0;
        UInt32 size = sizeof(AudioDeviceID);
        AudioObjectPropertyAddress addr = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &defaultOutput) == noErr) {
            outputDeviceId_ = defaultOutput;
            std::cout << "[AudioDeviceManager] Detected System Default Output Device ID: " << defaultOutput << std::endl;
        }
    }

    if (outputDeviceId_ > 0) {
        AudioDeviceID devId = static_cast<AudioDeviceID>(outputDeviceId_);
        AudioDeviceIOProcID procId = nullptr;
        OSStatus status = AudioDeviceCreateIOProcID(devId, CoreAudioIOCallback, this, &procId);
        if (status == noErr && procId) {
            audioProcHandle_ = reinterpret_cast<uintptr_t>(procId);
            AudioDeviceStart(devId, procId);
            isRunning_.store(true);
            std::cout << "[AudioDeviceManager] CoreAudio Hardware IOProc started on device " << devId << std::endl;
            return true;
        }
    }
#endif

    isRunning_.store(true);
    std::cout << "[AudioDeviceManager] Audio DSP Engine Started." << std::endl;
    return true;
}

void AudioDeviceManager::stop() {
    if (!isRunning_.load()) return;
    isRunning_.store(false);

#if defined(__APPLE__)
    if (outputDeviceId_ > 0 && audioProcHandle_ != 0) {
        AudioDeviceID devId = static_cast<AudioDeviceID>(outputDeviceId_);
        auto procId = reinterpret_cast<AudioDeviceIOProcID>(audioProcHandle_);
        AudioDeviceStop(devId, procId);
        AudioDeviceDestroyIOProcID(devId, procId);
        audioProcHandle_ = 0;
    }
#endif
    std::cout << "[AudioDeviceManager] Audio DSP Engine Stopped." << std::endl;
}

void AudioDeviceManager::generateTestSignal(float* left, float* right, size_t numFrames) noexcept {
    const auto signal = testSignalType_.load(std::memory_order_relaxed);
    if (signal == TestSignalType::None) return;

    const float sampleRate = static_cast<float>(sampleRate_);

    if (signal == TestSignalType::Sine1kHz) {
        const float freq = 1000.0f;
        const float delta = 2.0f * std::numbers::pi_v<float> * freq / sampleRate;
        for (size_t i = 0; i < numFrames; ++i) {
            float val = 0.4f * std::sin(sinePhase_);
            left[i] += val;
            right[i] += val;
            sinePhase_ += delta;
            if (sinePhase_ >= 2.0f * std::numbers::pi_v<float>) sinePhase_ -= 2.0f * std::numbers::pi_v<float>;
        }
    } else if (signal == TestSignalType::PinkNoise) {
        for (size_t i = 0; i < numFrames; ++i) {
            pinkNoiseState_ = pinkNoiseState_ * 1664525u + 1013904223u;
            float r1 = static_cast<float>(pinkNoiseState_) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
            pinkNoiseState_ = pinkNoiseState_ * 1664525u + 1013904223u;
            float r2 = static_cast<float>(pinkNoiseState_) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
            left[i] += 0.2f * r1;
            right[i] += 0.2f * r2;
        }
    } else if (signal == TestSignalType::SineSweep) {
        for (size_t i = 0; i < numFrames; ++i) {
            float sweepFreq = 40.0f + 15000.0f * (0.5f + 0.5f * std::sin(sweepPhase_ * 0.05f));
            float delta = 2.0f * std::numbers::pi_v<float> * sweepFreq / sampleRate;
            float val = 0.3f * std::sin(sinePhase_);
            left[i] += val;
            right[i] += val;
            sinePhase_ += delta;
            sweepPhase_ += 0.001f;
            if (sinePhase_ >= 2.0f * std::numbers::pi_v<float>) sinePhase_ -= 2.0f * std::numbers::pi_v<float>;
        }
    }
}

void AudioDeviceManager::processAudioBlock(float* left, float* right, size_t numFrames) noexcept {
    if (!left || !right || numFrames == 0) return;

    // 1. Generate test signal if enabled
    generateTestSignal(left, right, numFrames);

    // 2. Run Equalizer Filters Cascade
    if (eqEngine_) {
        eqEngine_->processStereo(left, right, numFrames);
    }

    // 3. Run Pro Effects (Volume Boost, Limiter, Balance, 3D Surround, Bass/Treble Enhancer)
    if (effectsEngine_) {
        effectsEngine_->processStereo(left, right, numFrames);
    }

    // 4. Feed Spectrum Analyzer & VU Meter
    if (spectrumAnalyzer_) {
        spectrumAnalyzer_->pushSamples(left, right, numFrames);
    }
}

} // namespace eqmax::audio
