#include "PresetManager.hpp"
#include <fstream>
#include <iostream>

namespace eqmax::preset {

PresetManager::PresetManager() {
    loadFactoryPresets();
    scanUserPresets();
}

void PresetManager::loadFactoryPresets() {
    // 1. Flat
    presets_["Flat"] = Preset{
        "Flat", dsp::EQMode::TenBand,
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        0.0f, 1.0f, 0.0f, 0.0f, true
    };

    // 2. Bass Booster
    presets_["Bass Booster"] = Preset{
        "Bass Booster", dsp::EQMode::TenBand,
        { 7.0f, 6.0f, 5.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        0.0f, 1.0f, 3.0f, 0.0f, true
    };

    // 3. Treble Booster
    presets_["Treble Booster"] = Preset{
        "Treble Booster", dsp::EQMode::TenBand,
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 3.0f, 5.0f, 6.5f, 7.5f },
        0.0f, 1.0f, 0.0f, 3.0f, true
    };

    // 4. Rock
    presets_["Rock"] = Preset{
        "Rock", dsp::EQMode::TenBand,
        { 5.0f, 3.5f, 2.0f, -1.0f, -2.0f, -1.0f, 2.0f, 3.5f, 5.0f, 6.0f },
        2.0f, 1.2f, 2.0f, 2.0f, true
    };

    // 5. Pop
    presets_["Pop"] = Preset{
        "Pop", dsp::EQMode::TenBand,
        { -1.5f, -1.0f, 0.0f, 2.0f, 4.0f, 4.0f, 2.0f, 0.0f, -1.0f, -1.5f },
        1.0f, 1.1f, 0.0f, 1.0f, true
    };

    // 6. Jazz
    presets_["Jazz"] = Preset{
        "Jazz", dsp::EQMode::TenBand,
        { 3.5f, 2.5f, 1.5f, 2.0f, -1.5f, -1.5f, 0.0f, 1.5f, 2.5f, 3.5f },
        0.0f, 1.15f, 1.0f, 1.0f, true
    };

    // 7. Electronic / Dance
    presets_["Electronic"] = Preset{
        "Electronic", dsp::EQMode::TenBand,
        { 7.5f, 6.0f, 3.0f, 0.0f, -2.0f, 1.5f, 3.0f, 4.0f, 5.5f, 6.5f },
        3.0f, 1.3f, 4.0f, 2.0f, true
    };

    // 8. Vocal Booster
    presets_["Vocal Booster"] = Preset{
        "Vocal Booster", dsp::EQMode::TenBand,
        { -2.0f, -1.5f, -1.0f, 1.0f, 3.5f, 4.5f, 3.5f, 1.5f, 0.0f, -1.0f },
        1.0f, 1.0f, 0.0f, 1.0f, true
    };

    // 9. Classical
    presets_["Classical"] = Preset{
        "Classical", dsp::EQMode::TenBand,
        { 4.0f, 3.0f, 2.0f, 1.5f, -1.0f, -1.0f, 0.0f, 1.5f, 2.5f, 3.5f },
        0.0f, 1.1f, 0.0f, 0.0f, true
    };

    // 10. Acoustic
    presets_["Acoustic"] = Preset{
        "Acoustic", dsp::EQMode::TenBand,
        { 3.0f, 2.0f, 1.0f, 1.5f, 2.0f, 1.5f, 2.5f, 3.0f, 2.0f, 1.5f },
        0.0f, 1.1f, 1.0f, 1.0f, true
    };

    // 11. Gaming / Movies
    presets_["Gaming / Movies"] = Preset{
        "Gaming / Movies", dsp::EQMode::TenBand,
        { 6.0f, 4.5f, 2.0f, 0.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f },
        4.0f, 1.5f, 3.0f, 2.0f, true
    };
}

std::filesystem::path PresetManager::getPresetsDirectory() const {
    const char* home = std::getenv("HOME");
    std::filesystem::path dir = home ? home : ".";
    dir /= ".config/eqMax/presets";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void PresetManager::scanUserPresets() {
    auto dir = getPresetsDirectory();
    if (!std::filesystem::exists(dir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".eqpreset") {
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            Preset p;
            file >> p.name;
            int modeInt = 1;
            file >> modeInt;
            p.mode = static_cast<dsp::EQMode>(modeInt);

            size_t count = 10;
            file >> count;
            p.gains.resize(count);
            for (size_t i = 0; i < count; ++i) {
                file >> p.gains[i];
            }
            file >> p.volumeBoostdB >> p.spatialWidth >> p.bassBoostdB >> p.trebleBoostdB;
            p.isFactory = false;

            if (!p.name.empty()) {
                presets_[p.name] = p;
            }
        }
    }
}

std::vector<std::string> PresetManager::getPresetNames() const {
    std::vector<std::string> names;
    for (const auto& [name, p] : presets_) {
        names.push_back(name);
    }
    return names;
}

std::optional<Preset> PresetManager::getPreset(const std::string& name) const {
    auto it = presets_.find(name);
    if (it != presets_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool PresetManager::saveUserPreset(const Preset& preset) {
    if (preset.name.empty()) return false;
    presets_[preset.name] = preset;

    auto dir = getPresetsDirectory();
    auto filePath = dir / (preset.name + ".eqpreset");

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << preset.name << "\n";
    file << static_cast<int>(preset.mode) << "\n";
    file << preset.gains.size() << "\n";
    for (float g : preset.gains) {
        file << g << " ";
    }
    file << "\n";
    file << preset.volumeBoostdB << " " << preset.spatialWidth << " "
         << preset.bassBoostdB << " " << preset.trebleBoostdB << "\n";

    return true;
}

bool PresetManager::deleteUserPreset(const std::string& name) {
    auto it = presets_.find(name);
    if (it == presets_.end() || it->second.isFactory) return false;

    presets_.erase(it);
    auto dir = getPresetsDirectory();
    auto filePath = dir / (name + ".eqpreset");
    std::error_code ec;
    return std::filesystem::remove(filePath, ec);
}

} // namespace eqmax::preset
