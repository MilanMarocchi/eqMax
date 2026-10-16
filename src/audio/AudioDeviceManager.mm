#include "AudioDeviceManager.hpp"
#include <iostream>
#include <cmath>
#include <numbers>
#include <set>
#include <array>
#include <algorithm>

#if defined(__APPLE__)
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <AudioToolbox/AudioToolbox.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <unistd.h>
#endif

namespace eqmax::audio {

#if defined(__APPLE__)

namespace {

constexpr size_t kMaxChunkFrames = 4096;
constexpr int64_t kRoutingPollIntervalNs = 500 * NSEC_PER_MSEC;

AudioObjectPropertyAddress globalAddress(AudioObjectPropertySelector selector,
                                         AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal) {
    return { selector, scope, kAudioObjectPropertyElementMain };
}

AudioObjectID defaultOutputDevice() {
    auto addr = globalAddress(kAudioHardwarePropertyDefaultOutputDevice);
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &device);
    return device;
}

NSString* deviceUID(AudioObjectID device) {
    auto addr = globalAddress(kAudioDevicePropertyDeviceUID);
    CFStringRef uid = nullptr;
    UInt32 size = sizeof(uid);
    if (AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &uid) != noErr || !uid) return nil;
    return (__bridge_transfer NSString*)uid;
}

UInt32 streamCount(AudioObjectID device, AudioObjectPropertyScope scope) {
    auto addr = globalAddress(kAudioDevicePropertyStreams, scope);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) != noErr) return 0;
    return size / sizeof(AudioStreamID);
}

UInt32 channelCount(AudioObjectID device, AudioObjectPropertyScope scope) {
    auto addr = globalAddress(kAudioDevicePropertyStreamConfiguration, scope);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) != noErr || size == 0) return 0;
    std::vector<uint8_t> storage(size);
    auto* list = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, list) != noErr) return 0;
    UInt32 channels = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i) channels += list->mBuffers[i].mNumberChannels;
    return channels;
}

AudioObjectID processObjectForPid(pid_t pid) {
    auto addr = globalAddress(kAudioHardwarePropertyTranslatePIDToProcessObject);
    AudioObjectID object = kAudioObjectUnknown;
    UInt32 size = sizeof(object);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, sizeof(pid), &pid, &size, &object);
    return object;
}

} // namespace

struct AudioDeviceManager::Impl {
    struct TapSlot {
        AudioObjectID tapId{kAudioObjectUnknown};
        AppGainControl* gain{nullptr}; // nullptr = unity gain ("all other apps" tap)
        float currentGain{1.0f};
    };

    explicit Impl(AudioDeviceManager* owner) : owner(owner), mixL(kMaxChunkFrames), mixR(kMaxChunkFrames) {
        queue = dispatch_queue_create("com.eqmax.audio-routing", DISPATCH_QUEUE_SERIAL);
    }

    AudioDeviceManager* owner;
    dispatch_queue_t queue;
    dispatch_source_t pollTimer{nil};
    AudioObjectPropertyListenerBlock routingListener{nil};

    // Requested configuration (control queue only)
    uint32_t requestedOutputDevice{0}; // 0 = follow system default

    // Last attempted routing configuration (control queue only)
    AudioObjectID routedOutputDevice{kAudioObjectUnknown};
    std::set<AudioObjectID> routedProcesses;
    bool hasRoutingAttempt{false};

    // Active CoreAudio objects. Only modified while the IOProc is stopped.
    AudioObjectID aggregateId{kAudioObjectUnknown};
    AudioDeviceIOProcID ioProcId{nullptr};
    std::vector<TapSlot> slots;
    std::array<UInt32, 2> stereoChannels{0, 1}; // 0-based channel indices across the aggregate's output streams

    // Real-time scratch buffers
    std::vector<float> mixL;
    std::vector<float> mixR;

    // --- Control queue operations ---
    void checkRouting();
    bool buildRouting(AudioObjectID outputDevice, const std::vector<AudioProcessInfo>& tapped);
    void teardownRouting();
    void addListeners();
    void removeListeners();

    // --- Real-time ---
    void render(const AudioBufferList* input, AudioBufferList* output) noexcept;
};

static OSStatus RoutingIOProc(
    AudioObjectID inDevice,
    const AudioTimeStamp* inNow,
    const AudioBufferList* inInputData,
    const AudioTimeStamp* inInputTime,
    AudioBufferList* outOutputData,
    const AudioTimeStamp* inOutputTime,
    void* inClientData) {
    static_cast<AudioDeviceManager::Impl*>(inClientData)->render(inInputData, outOutputData);
    return noErr;
}

void AudioDeviceManager::Impl::render(const AudioBufferList* input, AudioBufferList* output) noexcept {
    if (!output || output->mNumberBuffers == 0) return;

    // Locate the stereo output channels inside the aggregate's output buffers
    struct ChannelLocation { float* data{nullptr}; UInt32 stride{1}; UInt32 offset{0}; UInt32 frames{0}; };
    std::array<ChannelLocation, 2> outs{};
    UInt32 totalFrames = 0;
    UInt32 channelBase = 0;
    for (UInt32 b = 0; b < output->mNumberBuffers; ++b) {
        AudioBuffer& buf = output->mBuffers[b];
        UInt32 channels = std::max<UInt32>(1, buf.mNumberChannels);
        if (buf.mData && buf.mDataByteSize > 0) {
            std::fill_n(static_cast<float*>(buf.mData), buf.mDataByteSize / sizeof(float), 0.0f);
            UInt32 frames = buf.mDataByteSize / (sizeof(float) * channels);
            totalFrames = std::max(totalFrames, frames);
            for (size_t c = 0; c < outs.size(); ++c) {
                if (stereoChannels[c] >= channelBase && stereoChannels[c] < channelBase + channels) {
                    outs[c] = { static_cast<float*>(buf.mData), channels, stereoChannels[c] - channelBase, frames };
                }
            }
        }
        channelBase += channels;
    }
    if (totalFrames == 0) return;

    // Tap streams follow the output device's own input streams, in tap-list order
    const UInt32 tapCount = static_cast<UInt32>(slots.size());
    const bool hasTaps = input && tapCount > 0 && input->mNumberBuffers >= tapCount;
    const UInt32 firstTap = hasTaps ? input->mNumberBuffers - tapCount : 0;

    for (UInt32 offset = 0; offset < totalFrames; offset += kMaxChunkFrames) {
        const UInt32 n = std::min<UInt32>(kMaxChunkFrames, totalFrames - offset);
        std::fill_n(mixL.data(), n, 0.0f);
        std::fill_n(mixR.data(), n, 0.0f);

        if (hasTaps) {
            for (UInt32 i = 0; i < tapCount; ++i) {
                TapSlot& slot = slots[i];
                const AudioBuffer& buf = input->mBuffers[firstTap + i];
                const float targetGain = slot.gain ? slot.gain->effectiveGain() : 1.0f;
                if (!buf.mData || buf.mDataByteSize == 0) {
                    slot.currentGain = targetGain;
                    continue;
                }

                const auto* samples = static_cast<const float*>(buf.mData);
                const UInt32 channels = std::max<UInt32>(1, buf.mNumberChannels);
                const UInt32 available = buf.mDataByteSize / (sizeof(float) * channels);
                if (offset >= available) continue;
                const UInt32 frames = std::min(n, available - offset);
                const UInt32 rightOffset = channels > 1 ? 1 : 0;

                // Linear gain ramp across the chunk to avoid zipper noise
                float gain = slot.currentGain;
                const float step = (targetGain - gain) / static_cast<float>(n);
                for (UInt32 f = 0; f < frames; ++f) {
                    gain += step;
                    const UInt32 frame = (offset + f) * channels;
                    mixL[f] += samples[frame] * gain;
                    mixR[f] += samples[frame + rightOffset] * gain;
                }
                slot.currentGain = targetGain;
            }
        }

        // Test Generator, EQ Filters, Volume Boost/Attenuation, Limiter, Spectrum Analyzer
        owner->processAudioBlock(mixL.data(), mixR.data(), n);

        const bool sameChannel = outs[0].data == outs[1].data && outs[0].offset == outs[1].offset;
        for (size_t c = 0; c < outs.size(); ++c) {
            const ChannelLocation& loc = outs[c];
            if (!loc.data || offset >= loc.frames) continue;
            const UInt32 frames = std::min(n, loc.frames - offset);
            const float* mix = (c == 0) ? mixL.data() : mixR.data();
            for (UInt32 f = 0; f < frames; ++f) {
                const float sample = sameChannel ? 0.5f * (mixL[f] + mixR[f]) : mix[f];
                loc.data[(offset + f) * loc.stride + loc.offset] = sample;
            }
            if (sameChannel) break;
        }
    }
}

void AudioDeviceManager::Impl::checkRouting() {
    if (!owner->isRunning()) return;

    AudioObjectID outputDevice = requestedOutputDevice != 0 ? requestedOutputDevice : defaultOutputDevice();

    // Apps whose volume/mute the user changed get a dedicated tap; everything else is captured
    // by a single "all other processes" tap at unity gain.
    std::vector<AudioProcessInfo> tapped;
    std::set<AudioObjectID> tappedIds;
    for (auto& proc : owner->appVolumeManager_->getAudioProcesses()) {
        if (owner->appVolumeManager_->isCustomized(proc.appKey)) {
            tappedIds.insert(proc.objectId);
            tapped.push_back(std::move(proc));
        }
    }

    if (hasRoutingAttempt && outputDevice == routedOutputDevice && tappedIds == routedProcesses) return;

    teardownRouting();
    hasRoutingAttempt = true;
    routedOutputDevice = outputDevice;
    routedProcesses = std::move(tappedIds);

    if (outputDevice == kAudioObjectUnknown) {
        std::cerr << "[AudioDeviceManager] No output device available for routing." << std::endl;
        return;
    }
    if (!buildRouting(outputDevice, tapped)) {
        teardownRouting();
    }
}

bool AudioDeviceManager::Impl::buildRouting(AudioObjectID outputDevice, const std::vector<AudioProcessInfo>& tapped) {
    if (!@available(macOS 14.2, *)) {
        std::cerr << "[AudioDeviceManager] System audio routing requires macOS 14.2 or newer." << std::endl;
        return false;
    }

    NSString* outputUID = deviceUID(outputDevice);
    if (!outputUID) {
        std::cerr << "[AudioDeviceManager] Could not read UID of output device " << outputDevice << std::endl;
        return false;
    }

    // Our own output must never be captured, or it would feed back into itself.
    AudioObjectID selfProcess = processObjectForPid(getpid());
    if (selfProcess == kAudioObjectUnknown) {
        std::cerr << "[AudioDeviceManager] Could not resolve own audio process object; refusing to create a global tap." << std::endl;
        return false;
    }

    NSMutableArray<NSDictionary*>* tapList = [NSMutableArray array];
    NSMutableArray<NSNumber*>* excluded = [NSMutableArray arrayWithObject:@(selfProcess)];

    auto createTap = [&](CATapDescription* description, AppGainControl* gain) -> bool {
        description.privateTap = YES;
        description.muteBehavior = CATapMutedWhenTapped;
        description.UUID = [NSUUID UUID];
        AudioObjectID tapId = kAudioObjectUnknown;
        OSStatus status = AudioHardwareCreateProcessTap(description, &tapId);
        if (status != noErr || tapId == kAudioObjectUnknown) {
            std::cerr << "[AudioDeviceManager] Failed to create process tap '" << [description.name UTF8String]
                      << "' (OSStatus " << status << ")" << std::endl;
            return false;
        }
        slots.push_back({ tapId, gain, gain ? gain->effectiveGain() : 1.0f });
        [tapList addObject:@{
            @kAudioSubTapUIDKey: description.UUID.UUIDString,
            @kAudioSubTapDriftCompensationKey: @YES,
        }];
        return true;
    };

    for (const auto& proc : tapped) {
        CATapDescription* description = [[CATapDescription alloc] initStereoMixdownOfProcesses:@[ @(proc.objectId) ]];
        description.name = [NSString stringWithFormat:@"eqMax: %s (pid %d)", proc.appName.c_str(), proc.pid];
        if (createTap(description, owner->appVolumeManager_->gainControl(proc.appKey))) {
            [excluded addObject:@(proc.objectId)];
        }
    }

    CATapDescription* globalDescription = [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:excluded];
    globalDescription.name = @"eqMax: All Other Apps";
    if (!createTap(globalDescription, nullptr)) return false;

    NSDictionary* aggregateDescription = @{
        @kAudioAggregateDeviceNameKey: @"eqMax Router",
        @kAudioAggregateDeviceUIDKey: [NSString stringWithFormat:@"com.eqmax.router.%@", [NSUUID UUID].UUIDString],
        @kAudioAggregateDeviceMainSubDeviceKey: outputUID,
        @kAudioAggregateDeviceIsPrivateKey: @YES,
        @kAudioAggregateDeviceIsStackedKey: @NO,
        @kAudioAggregateDeviceTapAutoStartKey: @YES,
        @kAudioAggregateDeviceSubDeviceListKey: @[ @{ @kAudioSubDeviceUIDKey: outputUID } ],
        @kAudioAggregateDeviceTapListKey: tapList,
    };

    OSStatus status = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggregateDescription, &aggregateId);
    if (status != noErr || aggregateId == kAudioObjectUnknown) {
        std::cerr << "[AudioDeviceManager] Failed to create routing aggregate device (OSStatus " << status << ")" << std::endl;
        aggregateId = kAudioObjectUnknown;
        return false;
    }

    // Map the device's preferred stereo pair (1-based) onto the aggregate's output channels
    stereoChannels = { 0, 1 };
    {
        auto addr = globalAddress(kAudioDevicePropertyPreferredChannelsForStereo, kAudioObjectPropertyScopeOutput);
        std::array<UInt32, 2> preferred{ 1, 2 };
        UInt32 size = sizeof(preferred);
        const UInt32 outputChannels = channelCount(aggregateId, kAudioObjectPropertyScopeOutput);
        if (AudioObjectGetPropertyData(outputDevice, &addr, 0, nullptr, &size, preferred.data()) == noErr
            && preferred[0] >= 1 && preferred[1] >= 1
            && preferred[0] <= outputChannels && preferred[1] <= outputChannels) {
            stereoChannels = { preferred[0] - 1, preferred[1] - 1 };
        } else if (outputChannels == 1) {
            stereoChannels = { 0, 0 };
        }
    }

    {
        auto addr = globalAddress(kAudioDevicePropertyNominalSampleRate);
        Float64 rate = 0.0;
        UInt32 size = sizeof(rate);
        if (AudioObjectGetPropertyData(aggregateId, &addr, 0, nullptr, &size, &rate) == noErr && rate > 0.0) {
            owner->sampleRate_.store(static_cast<uint32_t>(rate), std::memory_order_relaxed);
            if (owner->eqEngine_) owner->eqEngine_->setSampleRate(static_cast<float>(rate));
            if (owner->effectsEngine_) owner->effectsEngine_->setSampleRate(static_cast<float>(rate));
        }
    }

    status = AudioDeviceCreateIOProcID(aggregateId, RoutingIOProc, this, &ioProcId);
    if (status != noErr || !ioProcId) {
        std::cerr << "[AudioDeviceManager] Failed to create IOProc (OSStatus " << status << ")" << std::endl;
        ioProcId = nullptr;
        return false;
    }

    // Disable the output device's own input streams (e.g. an interface's microphones) for our IOProc,
    // so only the tap streams are captured.
    const UInt32 inputStreams = streamCount(aggregateId, kAudioObjectPropertyScopeInput);
    if (inputStreams > slots.size()) {
        const UInt32 deviceInputStreams = inputStreams - static_cast<UInt32>(slots.size());
        std::vector<uint8_t> storage(offsetof(AudioHardwareIOProcStreamUsage, mStreamIsOn) + sizeof(UInt32) * inputStreams);
        auto* usage = reinterpret_cast<AudioHardwareIOProcStreamUsage*>(storage.data());
        usage->mIOProc = reinterpret_cast<void*>(ioProcId);
        usage->mNumberStreams = inputStreams;
        for (UInt32 i = 0; i < inputStreams; ++i) usage->mStreamIsOn[i] = i >= deviceInputStreams ? 1 : 0;
        auto addr = globalAddress(kAudioDevicePropertyIOProcStreamUsage, kAudioObjectPropertyScopeInput);
        AudioObjectSetPropertyData(aggregateId, &addr, 0, nullptr, static_cast<UInt32>(storage.size()), usage);
    }

    status = AudioDeviceStart(aggregateId, ioProcId);
    if (status != noErr) {
        std::cerr << "[AudioDeviceManager] Failed to start routing IOProc (OSStatus " << status << ")" << std::endl;
        return false;
    }

    owner->routingActive_.store(true, std::memory_order_relaxed);
    std::cout << "[AudioDeviceManager] Routing system audio to device " << outputDevice
              << " (" << [outputUID UTF8String] << ") at " << owner->currentSampleRate() << " Hz with "
              << tapped.size() << " per-app tap(s) + global tap, stereo channels "
              << stereoChannels[0] + 1 << "/" << stereoChannels[1] + 1 << std::endl;
    return true;
}

void AudioDeviceManager::Impl::teardownRouting() {
    owner->routingActive_.store(false, std::memory_order_relaxed);
    if (aggregateId != kAudioObjectUnknown && ioProcId) {
        AudioDeviceStop(aggregateId, ioProcId);
        AudioDeviceDestroyIOProcID(aggregateId, ioProcId);
    }
    ioProcId = nullptr;

    if (aggregateId != kAudioObjectUnknown) {
        AudioHardwareDestroyAggregateDevice(aggregateId);
        aggregateId = kAudioObjectUnknown;
    }

    if (@available(macOS 14.2, *)) {
        for (const auto& slot : slots) {
            AudioHardwareDestroyProcessTap(slot.tapId);
        }
    }
    slots.clear();
}

void AudioDeviceManager::Impl::addListeners() {
    Impl* impl = this;
    routingListener = ^(UInt32, const AudioObjectPropertyAddress*) {
        impl->checkRouting();
    };
    for (AudioObjectPropertySelector selector : { kAudioHardwarePropertyDefaultOutputDevice,
                                                  kAudioHardwarePropertyProcessObjectList,
                                                  kAudioHardwarePropertyDevices }) {
        auto addr = globalAddress(selector);
        AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &addr, queue, routingListener);
    }

    // Poll for volume/mute customizations that require a dedicated tap
    pollTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(pollTimer, dispatch_time(DISPATCH_TIME_NOW, kRoutingPollIntervalNs),
                              kRoutingPollIntervalNs, 50 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(pollTimer, ^{
        impl->checkRouting();
    });
    dispatch_resume(pollTimer);
}

void AudioDeviceManager::Impl::removeListeners() {
    if (routingListener) {
        for (AudioObjectPropertySelector selector : { kAudioHardwarePropertyDefaultOutputDevice,
                                                      kAudioHardwarePropertyProcessObjectList,
                                                      kAudioHardwarePropertyDevices }) {
            auto addr = globalAddress(selector);
            AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &addr, queue, routingListener);
        }
        routingListener = nil;
    }
    if (pollTimer) {
        dispatch_source_cancel(pollTimer);
        pollTimer = nil;
    }
}

#else

struct AudioDeviceManager::Impl {
    explicit Impl(AudioDeviceManager*) {}
};

#endif

AudioDeviceManager::AudioDeviceManager(
    std::shared_ptr<dsp::EqualizerEngine> eqEngine,
    std::shared_ptr<dsp::ProEffectsEngine> effectsEngine,
    std::shared_ptr<dsp::SpectrumAnalyzer> spectrumAnalyzer,
    std::shared_ptr<AppVolumeManager> appVolumeManager)
    : eqEngine_(std::move(eqEngine)),
      effectsEngine_(std::move(effectsEngine)),
      spectrumAnalyzer_(std::move(spectrumAnalyzer)),
      appVolumeManager_(appVolumeManager ? std::move(appVolumeManager) : std::make_shared<AppVolumeManager>()),
      impl_(std::make_unique<Impl>(this)) {
}

AudioDeviceManager::~AudioDeviceManager() {
    stop();
#if defined(__APPLE__)
    // Flush any listener callbacks queued before they were removed
    dispatch_sync(impl_->queue, ^{});
#endif
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
        return devices;
    }

    UInt32 deviceCount = dataSize / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> audioDevices(deviceCount);
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &dataSize, audioDevices.data());
    if (status != noErr) {
        return devices;
    }

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

        if (NSString* uid = deviceUID(devId)) {
            info.uid = [uid UTF8String];
        }
        // Our private tap aggregate is an implementation detail, never an output destination.
        if (info.uid.starts_with("com.eqmax.router.")) continue;

        // A device is an output/input only if it actually has streams in that scope
        info.isOutput = streamCount(devId, kAudioObjectPropertyScopeOutput) > 0;
        info.isInput = streamCount(devId, kAudioObjectPropertyScopeInput) > 0;

        Float64 rate = 0.0;
        dataSize = sizeof(rate);
        propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
        if (AudioObjectGetPropertyData(devId, &propertyAddress, 0, nullptr, &dataSize, &rate) == noErr && rate > 0.0) {
            info.sampleRate = static_cast<uint32_t>(rate);
        }

        if (info.name.empty()) {
            info.name = "Audio Device #" + std::to_string(devId);
        }

        devices.push_back(info);
    }
#else
    devices.push_back({ 1, "default_speaker", "System Speaker", false, true, 44100 });
#endif

    return devices;
}

bool AudioDeviceManager::setOutputDevice(uint32_t deviceId) {
#if defined(__APPLE__)
    Impl* impl = impl_.get();
    dispatch_sync(impl->queue, ^{
        if (impl->requestedOutputDevice == deviceId) return;
        impl->requestedOutputDevice = deviceId;
        std::cout << "[AudioDeviceManager] Selected Output Device ID: " << deviceId
                  << (deviceId == 0 ? " (system default)" : "") << std::endl;
        impl->checkRouting();
    });
#endif
    return true;
}

void AudioDeviceManager::requestAudioPermissions() {
#if defined(__APPLE__)
    // There is no public API to request system audio capture access up front; macOS prompts when a
    // process tap is first read. Point the user at the relevant privacy pane instead.
    NSURL* url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_AudioCapture"];
    [[NSWorkspace sharedWorkspace] openURL:url];
#endif
}

bool AudioDeviceManager::start() {
    if (isRunning_.load()) return true;
    isRunning_.store(true);

#if defined(__APPLE__)
    Impl* impl = impl_.get();
    dispatch_sync(impl->queue, ^{
        impl->hasRoutingAttempt = false;
        impl->addListeners();
        impl->checkRouting();
    });
#endif

    std::cout << "[AudioDeviceManager] Audio DSP Engine Started." << std::endl;
    return true;
}

void AudioDeviceManager::stop() {
    if (!isRunning_.load()) return;
    isRunning_.store(false);

#if defined(__APPLE__)
    Impl* impl = impl_.get();
    dispatch_sync(impl->queue, ^{
        impl->removeListeners();
        impl->teardownRouting();
        impl->hasRoutingAttempt = false;
    });
#endif
    std::cout << "[AudioDeviceManager] Audio DSP Engine Stopped." << std::endl;
}

void AudioDeviceManager::generateTestSignal(float* left, float* right, size_t numFrames) noexcept {
    const auto signal = testSignalType_.load(std::memory_order_relaxed);
    if (signal == TestSignalType::None) return;

    const float sampleRate = static_cast<float>(sampleRate_.load(std::memory_order_relaxed));

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
