#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace eqmax::audio {

struct AppAudioInfo {
    int pid{0};
    std::string appName;
    std::string bundleId;
    float volume{1.0f}; // 0.0 to 2.0 (200% boost)
    bool isMuted{false};
};

class AppVolumeManager {
public:
    AppVolumeManager();
    ~AppVolumeManager() = default;

    /**
     * @brief Query active running macOS user applications with audio capabilities.
     */
    std::vector<AppAudioInfo> getRunningApplications();

    /**
     * @brief Set volume for a specific application PID.
     * @param volume Range 0.0f (0%) to 2.0f (200%)
     */
    void setAppVolume(int pid, float volume);

    /**
     * @brief Mute or unmute a specific application.
     */
    void setAppMute(int pid, bool muted);

    /**
     * @brief Get effective volume gain for a specific PID (volume * (!isMuted)).
     */
    float getAppGain(int pid);

private:
    std::mutex mutex_;
    std::unordered_map<int, AppAudioInfo> appMap_;
};

} // namespace eqmax::audio
