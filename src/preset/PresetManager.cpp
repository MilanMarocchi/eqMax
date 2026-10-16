#include "PresetManager.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace eqmax::preset {

PresetManager::PresetManager(std::filesystem::path storageDirectory)
    : storageDirectory_(std::move(storageDirectory)) {
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

namespace {
bool validPreset(const Preset& p) {
    const auto name = QString::fromStdString(p.name);
    if (name.trimmed().isEmpty() || name.toUtf8().size() > 256 || name.contains('\n') || name.contains('\r')) return false;
    const auto inRange = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
    const size_t count = p.mode == dsp::EQMode::ThreeBand ? 3 : p.mode == dsp::EQMode::TenBand ? 10 :
                         p.mode == dsp::EQMode::ThirtyTwoBand ? 32 : 0;
    if (!count || p.gains.size() != count) return false;
    for (float gain : p.gains) if (!inRange(gain, -12, 12)) return false;
    return inRange(p.volumeBoostdB, -60, 24) && inRange(p.spatialWidth, 0, 2) &&
           inRange(p.bassBoostdB, 0, 12) && inRange(p.trebleBoostdB, 0, 12) && inRange(p.balance, -1, 1);
}
}

std::filesystem::path PresetManager::getPresetsDirectory() const {
    if (!storageDirectory_.empty()) return storageDirectory_;
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / ".config/eqMax/presets";
}

QByteArray PresetManager::serialize(const Preset& p) {
    if (!validPreset(p)) return {};
    QJsonArray gains;
    for (float gain : p.gains) gains.append(gain);
    return QJsonDocument(QJsonObject{
        {"version", 1}, {"name", QString::fromStdString(p.name)}, {"mode", static_cast<int>(p.mode)},
        {"gains", gains}, {"volumeBoostdB", p.volumeBoostdB}, {"spatialWidth", p.spatialWidth},
        {"bassBoostdB", p.bassBoostdB}, {"trebleBoostdB", p.trebleBoostdB},
        {"balance", p.balance}, {"limiterEnabled", p.limiterEnabled}
    }).toJson();
}

std::optional<Preset> PresetManager::deserialize(const QByteArray& data) {
    if (data.size() > 64 * 1024) return std::nullopt;
    Preset p;
    if (data.trimmed().startsWith('{')) {
        const auto doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return std::nullopt;
        const auto obj = doc.object();
        if (obj.value("version").toInt(-1) != 1 || !obj.value("mode").isDouble() ||
            !obj.value("gains").isArray()) return std::nullopt;
        const double mode = obj.value("mode").toDouble(-1);
        if (mode != 0 && mode != 1 && mode != 2) return std::nullopt;
        p.name = obj.value("name").toString().toStdString();
        p.mode = static_cast<dsp::EQMode>(static_cast<int>(mode));
        for (const auto& gain : obj.value("gains").toArray()) {
            if (!gain.isDouble()) return std::nullopt;
            p.gains.push_back(static_cast<float>(gain.toDouble()));
        }
        const auto readFloat = [&](const char* key, float fallback) {
            const auto value = obj.value(key);
            return value.isUndefined() ? fallback : value.isDouble() ? static_cast<float>(value.toDouble()) : NAN;
        };
        p.volumeBoostdB = readFloat("volumeBoostdB", 0);
        p.spatialWidth = readFloat("spatialWidth", 1);
        p.bassBoostdB = readFloat("bassBoostdB", 0);
        p.trebleBoostdB = readFloat("trebleBoostdB", 0);
        p.balance = readFloat("balance", 0);
        if (obj.contains("limiterEnabled") && !obj.value("limiterEnabled").isBool()) return std::nullopt;
        p.limiterEnabled = obj.value("limiterEnabled").toBool(true);
    } else {
        // Read the original line-based format, including names containing spaces.
        std::istringstream stream(data.toStdString());
        std::getline(stream, p.name);
        int mode = -1;
        size_t count = 0;
        if (!(stream >> mode >> count) || mode < 0 || mode > 2 || count > 32) return std::nullopt;
        p.mode = static_cast<dsp::EQMode>(mode);
        p.gains.resize(count);
        for (float& gain : p.gains) if (!(stream >> gain)) return std::nullopt;
        if (!(stream >> p.volumeBoostdB >> p.spatialWidth >> p.bassBoostdB >> p.trebleBoostdB)) return std::nullopt;
    }
    return validPreset(p) ? std::optional<Preset>(p) : std::nullopt;
}

std::optional<Preset> PresetManager::readPresetFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024) return std::nullopt;
    return deserialize(file.readAll());
}

bool PresetManager::writePresetFile(const QString& path, const Preset& p) {
    const auto data = serialize(p);
    if (data.isEmpty()) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

void PresetManager::scanUserPresets() {
    std::error_code ec;
    const auto dir = getPresetsDirectory();
    std::filesystem::directory_iterator it(dir, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        const auto& entry = *it;
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".eqpreset") continue;
        auto p = readPresetFile(QString::fromStdString(entry.path().string()));
        if (!p) continue;
        const auto existing = presets_.find(p->name);
        if (existing != presets_.end() && existing->second.isFactory) continue;
        presets_[p->name] = *p;
        userPaths_[p->name] = entry.path();
    }
}

std::vector<std::string> PresetManager::getPresetNames() const {
    std::vector<std::string> names;
    for (const auto& [name, p] : presets_) names.push_back(name);
    return names;
}

std::optional<Preset> PresetManager::getPreset(const std::string& name) const {
    auto it = presets_.find(name);
    return it != presets_.end() ? std::optional<Preset>(it->second) : std::nullopt;
}

bool PresetManager::saveUserPreset(const Preset& preset) {
    if (!validPreset(preset)) return false;
    auto existing = presets_.find(preset.name);
    if (existing != presets_.end() && existing->second.isFactory) return false;
    const auto dir = getPresetsDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;
    // Hash the name so punctuation, slashes, and Unicode never become filesystem paths.
    const auto filename = QCryptographicHash::hash(QByteArray::fromStdString(preset.name), QCryptographicHash::Sha256).toHex();
    const auto path = dir / (filename.toStdString() + ".eqpreset");
    if (!writePresetFile(QString::fromStdString(path.string()), preset)) return false;
    const auto oldPath = userPaths_.find(preset.name);
    if (oldPath != userPaths_.end() && oldPath->second != path) std::filesystem::remove(oldPath->second, ec);
    Preset saved = preset;
    saved.isFactory = false;
    presets_[saved.name] = saved;
    userPaths_[saved.name] = path;
    return true;
}

bool PresetManager::deleteUserPreset(const std::string& name) {
    auto it = presets_.find(name);
    auto path = userPaths_.find(name);
    if (it == presets_.end() || it->second.isFactory || path == userPaths_.end()) return false;
    std::error_code ec;
    std::filesystem::remove(path->second, ec);
    if (ec) return false;
    presets_.erase(it);
    userPaths_.erase(path);
    return true;
}

} // namespace eqmax::preset
