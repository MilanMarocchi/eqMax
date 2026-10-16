#include "MainWindow.hpp"
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QMouseEvent>
#include <cassert>
#include <iostream>

using namespace eqmax;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir dir;
    assert(dir.isValid());
    QCoreApplication::setOrganizationName("eqMax-tests");
    QCoreApplication::setApplicationName("ui");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    {
        auto eq = std::make_shared<dsp::EqualizerEngine>();
        ui::EQResponseWidget graph(eq);
        graph.resize(420, graph.sizeHint().height());
        graph.show();
        QTest::qWait(20);
        QObject::connect(&graph, &ui::EQResponseWidget::bandEdited, &graph, [&](size_t index, float gain) {
            eq->setBandGain(index, gain);
            graph.refresh();
        });
        const QRectF plot = QRectF(graph.rect()).adjusted(30, 12, -10, -22);
        const auto xForFrequency = [&](double frequency) {
            return qRound(plot.left() + std::log10(frequency / 20.0) / 3.0 * plot.width());
        };
        const int sampleX = xForFrequency(1100); // Between grid lines and band handles.
        const int aboveZero = qRound(plot.center().y() - 10);
        const int belowZero = qRound(plot.center().y() + 10);
        // Both boosts and cuts must shade only the area between the curve and 0 dB.
        // An accidental brush on the open response path used to erase this area.
        for (float gain : {9.0f, -9.0f}) {
            eq->setBandGain(5, gain);
            graph.refresh();
            const auto pixels = graph.grab().toImage();
            const auto shaded = pixels.pixelColor(sampleX, gain > 0 ? aboveZero : belowZero);
            const auto unshaded = pixels.pixelColor(sampleX, gain > 0 ? belowZero : aboveZero);
            assert(shaded.blue() > unshaded.blue() + 12);
        }
        eq->resetGains();
        graph.refresh();
        const auto flatPixels = graph.grab().toImage();
        assert(flatPixels.pixelColor(sampleX, aboveZero) == flatPixels.pixelColor(sampleX, belowZero));

        // A shelf's midpoint response differs from its requested gain. Its handle must sit
        // on the response, and grabbing it away from its centre must not change the gain.
        eq->setBandGain(0, 6);
        graph.refresh();
        dsp::BiquadFilter shelf;
        shelf.configure(dsp::FilterType::LowShelf, 32, 44100, 6, 1.414f);
        const QPoint handle(xForFrequency(32), qRound(plot.center().y() - shelf.magnitudedB(32) * plot.height() / 36));
        const QPoint grab = handle + QPoint(0, 3);
        const auto drag = [&](QPoint position, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
            QMouseEvent move(QEvent::MouseMove, QPointF(position), QPointF(graph.mapToGlobal(position)),
                             Qt::NoButton, Qt::LeftButton, modifiers);
            QApplication::sendEvent(&graph, &move);
        };
        QTest::mousePress(&graph, Qt::LeftButton, Qt::NoModifier, grab);
        drag(grab);
        assert(eq->bandGain(0) == 6);
        drag(grab - QPoint(0, 9));
        assert(eq->bandGain(0) == 9);
        drag(grab - QPoint(0, 9), Qt::ShiftModifier);
        assert(eq->bandGain(0) == 9); // Pressing Shift mid-drag must not jump.
        drag(grab - QPoint(0, 24), Qt::ShiftModifier);
        assert(eq->bandGain(0) == 10);
        QTest::mouseRelease(&graph, Qt::LeftButton, Qt::NoModifier, grab - QPoint(0, 24));
        assert(graph.grab().save(QCoreApplication::applicationDirPath() + "/eq-graph-preview.png"));
    }
    {
        audio::AppVolumeManager mixer;
        mixer.setAppVolume("com.example.player", 0.65f);
        mixer.setAppMute("com.example.player", true);
        mixer.saveSettings();
        audio::AppVolumeManager restored;
        assert(restored.isCustomized("com.example.player"));
        assert(std::abs(restored.gainControl("com.example.player")->volume.load() - 0.65f) < 0.001f);
        assert(restored.gainControl("com.example.player")->muted.load());
    }
    auto presets = std::make_shared<preset::PresetManager>(dir.path().toStdString() + "/presets");
    const auto makeWindow = [&](auto eq, auto effects) {
        return std::make_unique<ui::MainWindow>(eq, effects, std::make_shared<dsp::SpectrumAnalyzer>(),
                                               nullptr, nullptr, presets);
    };
    {
        auto eq = std::make_shared<dsp::EqualizerEngine>();
        auto effects = std::make_shared<dsp::ProEffectsEngine>();
        auto window = makeWindow(eq, effects);
        window->show();
        QTest::qWait(50);
        assert(window->width() == 480);
        auto* combo = window->findChild<QComboBox*>("presetSelector");
        auto* response = window->findChild<ui::EQResponseWidget*>();
        auto* volume = window->findChild<QSlider*>("masterVolume");
        auto* balance = window->findChild<QSlider*>("balance");
        assert(combo && response && volume && balance);
        window->loadPresetByName("Rock");
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        assert(combo->currentText() == "Rock");
        assert(eq->bandGain(0) == 5);
        // Editing the curve updates engine, band slider, and the Custom preset state.
        QTest::keyClick(response, Qt::Key_Up);
        assert(eq->bandGain(0) == 5.5f);
        assert(combo->currentText() == "Custom");
        assert(window->findChildren<ui::CustomSlider*>().front()->gaindB() == 5.5f);
        QTest::keyClick(response, Qt::Key_Home);
        assert(eq->bandGain(0) == 0);
        // Repeated mode changes and a saved three-band sound restore the right controls.
        auto basic = *presets->getPreset("Flat");
        basic.name = "Basic sound"; basic.mode = dsp::EQMode::ThreeBand; basic.gains = {2, -1, 3};
        assert(presets->saveUserPreset(basic));
        window->loadPresetByName(basic.name);
        assert(eq->mode() == dsp::EQMode::ThreeBand && eq->bandGain(2) == 3);
        window->loadPresetByName("Rock");
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        volume->setValue(-35);
        balance->setValue(-25);
        window->setBypass(true);
        assert(eq->isBypassed() && effects->isBypassed());
        window->findChild<ui::SectionCard*>("effects")->setCollapsed(true);
        window->setBypass(false);
        assert(!eq->isBypassed() && !effects->isBypassed());
        QTest::qWait(100); // Allow deferred deletion and native layout requests to settle before rendering.
        // Produce the same native Qt rendering used for visual QA, without starting audio capture.
        const auto screenshot = QCoreApplication::applicationDirPath() + "/ui-preview.png";
        assert(window->grab().save(screenshot));
    }
    {
        auto eq = std::make_shared<dsp::EqualizerEngine>();
        auto effects = std::make_shared<dsp::ProEffectsEngine>();
        auto window = makeWindow(eq, effects);
        assert(eq->mode() == dsp::EQMode::TenBand && eq->bandGain(0) == 5);
        assert(std::abs(effects->volumeBoostdB() + 3.5f) < 0.01f);
        assert(std::abs(effects->balance() + 0.25f) < 0.01f);
        assert(window->findChild<ui::SectionCard*>("effects")->isCollapsed());
        assert(window->findChild<QComboBox*>("presetSelector")->currentText() == "Custom");
    }
    std::cout << "Curve editing, presets, bypass, and session restore passed.\n";
}
