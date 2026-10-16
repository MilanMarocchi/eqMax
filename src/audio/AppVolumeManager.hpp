#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include <cstdint>

namespace eqmax::audio {

/**
 * @brief Lock-free per-application gain state, safe to read from the real-time audio thread.
 * Instances are never destroyed while the AppVolumeManager lives, so raw pointers stay valid.
 */
struct AppGainControl {
    std::atomic<float> volume{1.0f};  // 0.0 to 2.0 (200% boost)
    std::atomic<bool> muted{false};
    std::atomic<bool> customized{false}; // true once the user has touched this app's volume/mute

    [[nodiscard]] float effectiveGain() const noexcept {
        return muted.load(std::memory_order_relaxed) ? 0.0f : volume.load(std::memory_order_relaxed);
    }
};

/**
 * @brief A CoreAudio process object (a process connected to the audio server).
 */
struct AudioProcessInfo {
    uint32_t objectId{0};
    int pid{0};
    std::string appKey;   // Stable grouping key (owning app bundle id, or process bundle id / path)
    std::string appName;
    int appPid{0};        // PID of the owning GUI app (== pid for standalone processes)
    bool isUserApp{false};
    bool isRunningOutput{false};
};

struct AppAudioInfo {
    int pid{0};
    std::string appName;
    std::string bundleId;
    std::string appKey;
    float volume{1.0f}; // 0.0 to 2.0 (200% boost)
    bool isMuted{false};
    bool isPlaying{false};
};

class AppVolumeManager {
public:
    AppVolumeManager();
    ~AppVolumeManager() = default;

    /**
     * @brief Enumerate CoreAudio process objects (excluding this process), resolved to their owning app.
     */
    std::vector<AudioProcessInfo> getAudioProcesses();

    /**
     * @brief Applications that can produce audio, grouped by owning app (for the mixer UI).
     */
    std::vector<AppAudioInfo> getRunningApplications();

    /**
     * @brief Set volume for an application.
     * @param volume Range 0.0f (0%) to 2.0f (200%)
     */
    void setAppVolume(const std::string& appKey, float volume);

    /**
     * @brief Mute or unmute an application.
     */
    void setAppMute(const std::string& appKey, bool muted);

    /**
     * @brief Stable gain control for an app key (created on demand).
     */
    AppGainControl* gainControl(const std::string& appKey);

    /**
     * @brief Whether the user has changed volume/mute for this app, which requires a dedicated process tap.
     */
    bool isCustomized(const std::string& appKey);
    void saveSettings();

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<AppGainControl>> gains_;
};

} // namespace eqmax::audio
