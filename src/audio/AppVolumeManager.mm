#include "AppVolumeManager.hpp"
#include <algorithm>
#include <map>
#include <iostream>
#include <QSettings>
#include <cmath>

#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <CoreAudio/CoreAudio.h>
#include <libproc.h>
#include <unistd.h>
#endif

namespace eqmax::audio {

AppVolumeManager::AppVolumeManager() {
    QSettings settings;
    const int count = settings.beginReadArray("mixer/applications");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const auto key = settings.value("key").toString().toStdString();
        const float volume = settings.value("volume", 1).toFloat();
        if (key.empty() || !std::isfinite(volume)) continue;
        auto* gain = gainControl(key);
        gain->volume.store(std::clamp(volume, 0.0f, 2.0f));
        gain->muted.store(settings.value("muted", false).toBool());
        gain->customized.store(true);
    }
    settings.endArray();
}

void AppVolumeManager::saveSettings() {
    QSettings settings;
    std::lock_guard lock(mutex_);
    settings.remove("mixer/applications");
    settings.beginWriteArray("mixer/applications");
    int index = 0;
    for (const auto& [key, gain] : gains_) {
        if (!gain->customized.load()) continue;
        settings.setArrayIndex(index++);
        settings.setValue("key", QString::fromStdString(key));
        settings.setValue("volume", gain->volume.load());
        settings.setValue("muted", gain->muted.load());
    }
    settings.endArray();
}

#if defined(__APPLE__)
namespace {

std::vector<AudioObjectID> readProcessObjectList() {
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyProcessObjectList,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size) != noErr || size == 0) {
        return {};
    }
    std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, ids.data()) != noErr) {
        return {};
    }
    ids.resize(size / sizeof(AudioObjectID));
    return ids;
}

UInt32 readProcessUInt32(AudioObjectID objectId, AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress addr = { selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 value = 0;
    UInt32 size = sizeof(value);
    AudioObjectGetPropertyData(objectId, &addr, 0, nullptr, &size, &value);
    return value;
}

std::string readProcessBundleId(AudioObjectID objectId) {
    AudioObjectPropertyAddress addr = { kAudioProcessPropertyBundleID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    CFStringRef bundleId = nullptr;
    UInt32 size = sizeof(bundleId);
    if (AudioObjectGetPropertyData(objectId, &addr, 0, nullptr, &size, &bundleId) != noErr || !bundleId) {
        return {};
    }
    const char* identifier = [(__bridge NSString*)bundleId UTF8String];
    std::string result = identifier ? identifier : "";
    CFRelease(bundleId);
    return result;
}

std::string executablePath(pid_t pid) {
    char path[PROC_PIDPATHINFO_MAXSIZE] = {0};
    if (proc_pidpath(pid, path, sizeof(path)) <= 0) return {};
    return path;
}

// Resolve the GUI application that owns an audio process (e.g. "Google Chrome Helper" -> Google Chrome).
NSRunningApplication* owningApplication(pid_t pid, const std::string& exePath, const std::string& bundleId,
                                        NSArray<NSRunningApplication*>* regularApps) {
    for (NSRunningApplication* app in regularApps) {
        if (app.processIdentifier == pid) return app;
    }
    if (!exePath.empty()) {
        for (NSRunningApplication* app in regularApps) {
            NSString* bundlePath = app.bundleURL.path;
            if (bundlePath.length == 0) continue;
            std::string prefix = std::string([bundlePath UTF8String]) + "/";
            if (exePath.compare(0, prefix.size(), prefix) == 0) return app;
        }
    }
    if (!bundleId.empty()) {
        for (NSRunningApplication* app in regularApps) {
            NSString* appBundleId = app.bundleIdentifier;
            if (appBundleId.length == 0) continue;
            std::string prefix = std::string([appBundleId UTF8String]) + ".";
            if (bundleId.compare(0, prefix.size(), prefix) == 0) return app;
        }
    }
    return nil;
}

} // namespace
#endif

std::vector<AudioProcessInfo> AppVolumeManager::getAudioProcesses() {
    std::vector<AudioProcessInfo> result;

#if defined(__APPLE__)
    @autoreleasepool {
        NSMutableArray<NSRunningApplication*>* regularApps = [NSMutableArray array];
        for (NSRunningApplication* app in [[NSWorkspace sharedWorkspace] runningApplications]) {
            if (app.activationPolicy == NSApplicationActivationPolicyRegular) {
                [regularApps addObject:app];
            }
        }

        const pid_t selfPid = getpid();
        for (AudioObjectID objectId : readProcessObjectList()) {
            AudioProcessInfo info;
            info.objectId = objectId;
            info.pid = static_cast<int>(readProcessUInt32(objectId, kAudioProcessPropertyPID));
            if (info.pid == selfPid) continue;

            info.isRunningOutput = readProcessUInt32(objectId, kAudioProcessPropertyIsRunningOutput) != 0;

            const std::string bundleId = readProcessBundleId(objectId);
            const std::string exePath = executablePath(info.pid);

            if (NSRunningApplication* owner = owningApplication(info.pid, exePath, bundleId, regularApps)) {
                info.isUserApp = true;
                info.appPid = static_cast<int>(owner.processIdentifier);
                info.appName = owner.localizedName ? [owner.localizedName UTF8String] : "";
                if (owner.bundleIdentifier) {
                    info.appKey = [owner.bundleIdentifier UTF8String];
                } else if (owner.bundleURL.path) {
                    info.appKey = [owner.bundleURL.path UTF8String];
                }
            }

            if (info.appKey.empty()) {
                info.appPid = info.pid;
                info.appKey = !bundleId.empty() ? bundleId : exePath;
            }
            if (info.appKey.empty()) {
                info.appKey = "pid:" + std::to_string(info.pid);
            }
            if (info.appName.empty()) {
                if (bundleId == "com.apple.WebKit.GPU") {
                    info.appName = "WebKit Media (Safari, Mail, ...)";
                } else if (!exePath.empty()) {
                    info.appName = exePath.substr(exePath.find_last_of('/') + 1);
                } else {
                    info.appName = info.appKey;
                }
            }

            result.push_back(std::move(info));
        }
    }
#endif

    return result;
}

std::vector<AppAudioInfo> AppVolumeManager::getRunningApplications() {
    std::vector<AppAudioInfo> result;

#if defined(__APPLE__)
    // Group audio processes by owning app; hide background daemons unless they are currently playing.
    std::map<std::string, AppAudioInfo> grouped;
    for (const auto& proc : getAudioProcesses()) {
        auto [it, inserted] = grouped.try_emplace(proc.appKey);
        AppAudioInfo& app = it->second;
        if (inserted) {
            app.pid = proc.appPid;
            app.appName = proc.appName;
            app.bundleId = proc.appKey;
            app.appKey = proc.appKey;
        }
        app.isPlaying = app.isPlaying || proc.isRunningOutput;
        if (proc.isUserApp) app.pid = proc.appPid;
    }

    for (auto& [key, app] : grouped) {
        const AppGainControl* gain = gainControl(key);
        app.volume = gain->volume.load(std::memory_order_relaxed);
        app.isMuted = gain->muted.load(std::memory_order_relaxed);
        if (app.isPlaying || app.pid != 0) {
            result.push_back(app);
        }
    }

    // Drop background processes that are neither playing nor owned by a GUI app
    NSMutableSet<NSNumber*>* regularPids = [NSMutableSet set];
    for (NSRunningApplication* app in [[NSWorkspace sharedWorkspace] runningApplications]) {
        if (app.activationPolicy == NSApplicationActivationPolicyRegular) {
            [regularPids addObject:@(app.processIdentifier)];
        }
    }
    std::erase_if(result, [&](const AppAudioInfo& app) {
        return !app.isPlaying && ![regularPids containsObject:@(app.pid)] && !isCustomized(app.appKey);
    });

    std::stable_sort(result.begin(), result.end(), [](const AppAudioInfo& a, const AppAudioInfo& b) {
        if (a.isPlaying != b.isPlaying) return a.isPlaying;
        return a.appName < b.appName;
    });
#else
    result.push_back({ 101, "Google Chrome", "com.google.Chrome", "com.google.Chrome", 1.0f, false, false });
    result.push_back({ 102, "Spotify", "com.spotify.client", "com.spotify.client", 1.0f, false, false });
    result.push_back({ 103, "Apple Music", "com.apple.Music", "com.apple.Music", 1.0f, false, false });
#endif

    return result;
}

AppGainControl* AppVolumeManager::gainControl(const std::string& appKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& entry = gains_[appKey];
    if (!entry) entry = std::make_unique<AppGainControl>();
    return entry.get();
}

void AppVolumeManager::setAppVolume(const std::string& appKey, float volume) {
    AppGainControl* gain = gainControl(appKey);
    gain->volume.store(std::clamp(volume, 0.0f, 2.0f), std::memory_order_relaxed);
    gain->customized.store(true, std::memory_order_relaxed);
}

void AppVolumeManager::setAppMute(const std::string& appKey, bool muted) {
    AppGainControl* gain = gainControl(appKey);
    gain->muted.store(muted, std::memory_order_relaxed);
    gain->customized.store(true, std::memory_order_relaxed);
}

bool AppVolumeManager::isCustomized(const std::string& appKey) {
    return gainControl(appKey)->customized.load(std::memory_order_relaxed);
}

} // namespace eqmax::audio
