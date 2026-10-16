#include "PresetManager.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

using namespace eqmax;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    assert(dir.isValid());
    const auto storage = std::filesystem::path(dir.path().toStdString());
    preset::PresetManager manager(storage);
    auto sound = *manager.getPreset("Rock");
    sound.name = "Desk / Headphones – evening";
    sound.balance = -0.3f;
    sound.limiterEnabled = false;
    // Round-trip through disk, preserving a Unicode name with spaces and path punctuation.
    assert(manager.saveUserPreset(sound));
    preset::PresetManager reloaded(storage);
    auto loaded = reloaded.getPreset(sound.name);
    assert(loaded && !loaded->isFactory);
    assert(loaded->gains == sound.gains && loaded->balance == sound.balance && !loaded->limiterEnabled);
    assert(!manager.saveUserPreset(*manager.getPreset("Rock")));
    assert(manager.getPreset("Rock")->isFactory);

    // Export/import and the original file format both preserve multiword names.
    const auto exported = dir.filePath("export.eqpreset");
    assert(preset::PresetManager::writePresetFile(exported, sound));
    assert(preset::PresetManager::readPresetFile(exported)->name == sound.name);
    const auto legacy = preset::PresetManager::deserialize("My old preset\n0\n3\n1 2 3\n0 1 0 0\n");
    assert(legacy && legacy->name == "My old preset" && legacy->gains.size() == 3);
    assert(!preset::PresetManager::deserialize("Broken\n2\n999999999\n"));
    assert(!preset::PresetManager::deserialize("Broken\n7\n3\n0 0 0\n0 1 0 0\n"));
    assert(!preset::PresetManager::deserialize("Truncated\n0\n3\n1\n"));
    assert(!preset::PresetManager::deserialize(R"({"version":1,"name":"Bad","mode":0,"gains":[0,0,"oops"]})"));
    assert(!preset::PresetManager::deserialize(R"({"version":1,"name":"Bad","mode":0.5,"gains":[0,0,0]})"));
    sound.gains[0] = 99;
    assert(!manager.saveUserPreset(sound));
    assert(manager.getPreset(sound.name)->gains[0] != 99);
    sound.gains[0] = 1;

    // A failed write must not appear as a saved preset in memory.
    QFile blocker(dir.filePath("file"));
    assert(blocker.open(QIODevice::WriteOnly)); blocker.close();
    preset::PresetManager unwritable(storage / "file" / "child");
    assert(!unwritable.saveUserPreset(sound));
    assert(!unwritable.getPreset(sound.name));
    assert(reloaded.deleteUserPreset(sound.name));
    assert(!reloaded.getPreset(sound.name));
    assert(!reloaded.deleteUserPreset("Flat"));
    std::cout << "Preset validation, legacy import, atomic saves, and deletion passed.\n";
}
