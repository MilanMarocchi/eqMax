#include "AppVolumeManager.hpp"
#include <iostream>

#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#endif

namespace eqmax::audio {

AppVolumeManager::AppVolumeManager() = default;

std::vector<AppAudioInfo> AppVolumeManager::getRunningApplications() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AppAudioInfo> result;

#if defined(__APPLE__)
    NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
    NSArray<NSRunningApplication*>* runningApps = [workspace runningApplications];

    for (NSRunningApplication* app in runningApps) {
        // Filter active GUI user applications (Activation Policy Regular)
        if (app.activationPolicy == NSApplicationActivationPolicyRegular) {
            int pid = static_cast<int>(app.processIdentifier);
            NSString* name = app.localizedName;
            NSString* bundleId = app.bundleIdentifier;

            std::string appNameStr = name ? [name UTF8String] : "Unknown Application";
            std::string bundleIdStr = bundleId ? [bundleId UTF8String] : "";

            // Retain existing volume & mute settings if already tracked
            float currentVol = 1.0f;
            bool currentMute = false;
            auto it = appMap_.find(pid);
            if (it != appMap_.end()) {
                currentVol = it->second.volume;
                currentMute = it->second.isMuted;
            }

            AppAudioInfo info{ pid, appNameStr, bundleIdStr, currentVol, currentMute };
            appMap_[pid] = info;
            result.push_back(info);
        }
    }
#else
    result.push_back({ 101, "Google Chrome", "com.google.Chrome", 1.0f, false });
    result.push_back({ 102, "Spotify", "com.spotify.client", 1.0f, false });
    result.push_back({ 103, "Apple Music", "com.apple.Music", 1.0f, false });
#endif

    return result;
}

void AppVolumeManager::setAppVolume(int pid, float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    appMap_[pid].volume = std::clamp(volume, 0.0f, 2.0f);
}

void AppVolumeManager::setAppMute(int pid, bool muted) {
    std::lock_guard<std::mutex> lock(mutex_);
    appMap_[pid].isMuted = muted;
}

float AppVolumeManager::getAppGain(int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = appMap_.find(pid);
    if (it != appMap_.end()) {
        return it->second.isMuted ? 0.0f : it->second.volume;
    }
    return 1.0f; // Default 100% volume
}

} // namespace eqmax::audio
