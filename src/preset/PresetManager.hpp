#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <memory>
#include "../dsp/EqualizerEngine.hpp"

namespace eqmax::preset {

struct Preset {
    std::string name;
    dsp::EQMode mode{dsp::EQMode::TenBand};
    std::vector<float> gains;
    float volumeBoostdB{0.0f};
    float spatialWidth{1.0f};
    float bassBoostdB{0.0f};
    float trebleBoostdB{0.0f};
    bool isFactory{false};
};

class PresetManager {
public:
    PresetManager();
    ~PresetManager() = default;

    /**
     * @brief Load default factory presets.
     */
    void loadFactoryPresets();

    /**
     * @brief Scan and load user presets from local storage directory (~/.config/eqMax/presets).
     */
    void scanUserPresets();

    /**
     * @brief Get list of available preset names.
     */
    std::vector<std::string> getPresetNames() const;

    /**
     * @brief Get a preset by name.
     */
    std::optional<Preset> getPreset(const std::string& name) const;

    /**
     * @brief Save a user custom preset.
     */
    bool saveUserPreset(const Preset& preset);

    /**
     * @brief Delete a user custom preset.
     */
    bool deleteUserPreset(const std::string& name);

    /**
     * @brief Get storage directory path.
     */
    std::filesystem::path getPresetsDirectory() const;

private:
    std::map<std::string, Preset> presets_;
};

} // namespace eqmax::preset
