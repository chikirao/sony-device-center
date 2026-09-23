#include <QtTest>
#include "DeviceCenterController.h"
#include "TrayController.h"
#include "NotificationController.h"
#include "HotkeyManager.h"
#include "EqualizerLibrary.h"
#include "BluetoothWatcher.h"
#include "UpdateChecker.h"
#include "HubSettings.h"
#include "HubWindow.h"
#include "PeripheralModel.h"
#include "PeripheralSource.h"
#include "../support/FakeReleaseFetcher.h"
#include <QTemporaryDir>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QJSValue>
#include <QQuickWindow>
#include <QQuickItem>
#include <functional>
#include <QQuickStyle>
#include <QDir>
#include <QSettings>
#include "sony/core/DeviceService.h"
#include "sony/core/SimulatedDevice.h"
#include "sony/protocol/FrameCodec.h"
#include "../support/ReplyTransport.h"
#include <atomic>
#include <chrono>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
using namespace sony;
using namespace sony::devicecenter;
class SlowService : public core::IDeviceService {
public:
    void tick() override { std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
    std::vector<core::DiscoveredDevice> discoverDevices() override { return {}; }
    void connect(const transport::DeviceAddress&, std::string_view) override {}
    void disconnect() noexcept override {}
    bool isConnected() const noexcept override { return false; }
    core::SonyDevice* activeDevice() noexcept override { return nullptr; }
    protocol::DeviceStateSnapshot snapshot() const override { return std::make_shared<const protocol::DeviceState>(); }
};
// Counts wake-ups; never connects.
class WakeCountingService : public core::IDeviceService {
public:
    std::atomic<int> wakes{0};
    void wake() override { ++wakes; }
    std::vector<core::DiscoveredDevice> discoverDevices() override { return {}; }
    void connect(const transport::DeviceAddress&, std::string_view) override {}
    void disconnect() noexcept override {}
    bool isConnected() const noexcept override { return false; }
    core::SonyDevice* activeDevice() noexcept override { return nullptr; }
    protocol::DeviceStateSnapshot snapshot() const override { return std::make_shared<const protocol::DeviceState>(); }
};
class DeviceControllerTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QQuickStyle::setStyle("Basic"); QStandardPaths::setTestModeEnabled(true); }
    void extractedPagesLoad_data() {
        QTest::addColumn<QString>("model");
        QTest::addColumn<QString>("language");
        QTest::addColumn<QSize>("size");
        for (const auto& model : {"WH-1000XM5", "WF-1000XM5"})
            for (const auto& language : {"en", "ru"})
                // The narrow strip (sidebar folded), the old minimum, a big screen.
                for (const auto size : {QSize(460, 760), QSize(980, 660), QSize(1600, 1000)}) {
                    const auto name = QString("%1-%2-%3").arg(model, language).arg(size.width());
                    QTest::newRow(qPrintable(name)) << QString(model) << QString(language) << size;
                }
    }
    void extractedPagesLoad() {
        QFETCH(QString, model);
        QFETCH(QString, language);
        QFETCH(QSize, size);
        auto simulated = core::createSimulatedDevice(model.toStdString());
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        const auto previousLanguage = controller.currentLanguage();
        controller.setLanguage(language);
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError>& errors) {
            for (const auto& error : errors) warnings.append(error.toString());
        });
        TrayController tray(controller);
        HotkeyManager hotkeys(controller, tray, nullptr, "hotkeys-test");
        QTemporaryDir libraryDir;
        EqualizerLibrary eqLibrary(controller, libraryDir.path());
        // A newer release on offer, so the Settings card renders its busiest state.
        auto* fetcher = new test::FakeReleaseFetcher;
        fetcher->body = test::FakeReleaseFetcher::release("v9.9.9", {"sony-device-center-9.9.9-" + UpdateChecker::platformAssetSuffix()});
        UpdateChecker updates(SONY_DEVICE_CENTER_VERSION, fetcher);
        engine.rootContext()->setContextProperty("controller", &controller);
        engine.rootContext()->setContextProperty("hotkeys", &hotkeys);
        engine.rootContext()->setContextProperty("eqLibrary", &eqLibrary);
        engine.rootContext()->setContextProperty("updates", &updates);
        // The hub's list: the simulator's Sony sets plus the three fake
        // peripherals, exactly what --simulated shows.
        FakePeripheralSource peripheralSource;
        peripheralSource.setPeripherals(FakePeripheralSource::simulatedSet());
        PeripheralModel peripherals(controller, peripheralSource);
        engine.rootContext()->setContextProperty("peripherals", &peripherals);
        QTemporaryDir hubSettingsDir;
        HubSettings hubSettings(hubSettingsDir.path() + "/hub.ini");
        engine.rootContext()->setContextProperty("hubSettings", &hubSettings);
        // The hub card is tray-only; pretend there is one so it renders.
        engine.rootContext()->setContextProperty("trayAvailable", true);
        engine.rootContext()->setContextProperty("startHidden", false);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        controller.setLanguage(previousLanguage);
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join("\n")));
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(window);
        controller.setLanguage(language);
        window->resize(size);
        const auto previousTheme = controller.themeMode();
        const auto previousAnimations = controller.animationsEnabled();
        controller.setThemeMode("light");
        QTest::qWait(20);
        QCOMPARE(window->color(), QColor("#EDEDED"));
        // Built-in equalizer presets in Headphones Connect order.
        auto* equalizerPage = window->findChild<QObject*>("equalizerPage");
        QVERIFY(equalizerPage);
        QList<int> presetOrder;
        for (const auto& id : equalizerPage->property("presets").value<QJSValue>().toVariant().toList()) presetOrder << id.toInt();
        QCOMPARE(presetOrder, QList<int>({0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0xa0}));
        const auto output = qEnvironmentVariable("SONY_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            window->setProperty("navIndex", 6);
            QTest::qWait(400);
            QVERIFY(window->grabWindow().save(QString("%1/light-%2-%3-%4.png").arg(output, model, language).arg(size.width())));
        }
        // The Device Hub over the same engine: one row per model entry, the
        // connected set on top with its controls.
        HubWindow hub(engine, window);
        QVERIFY2(hub.isReady(), qPrintable(warnings.join("\n")));
        QTRY_VERIFY_WITH_TIMEOUT(peripherals.rowCount() >= 4, 5000);
        hub.open(QRect(window->x() + 300, window->y() + 500, 24, 24));
        QTRY_VERIFY(hub.isVisible());
        auto* hubList = hub.window()->findChild<QObject*>("hubList");
        QVERIFY(hubList);
        QTRY_COMPARE(hubList->property("count").toInt(), peripherals.rowCount());
        QCOMPARE(hub.window()->width(), 360);
        // Delegates are visual children of the list, not QObject children,
        // so findChild cannot see them; walk the item tree instead. They
        // come up a frame after the model, and the connected set's power
        // control is the sign they have.
        std::function<QQuickItem*(QQuickItem*, const QString&)> findItem = [&](QQuickItem* item, const QString& name) -> QQuickItem* {
            if (!item) return nullptr;
            if (item->objectName() == name) return item;
            for (auto* child : item->childItems())
                if (auto* found = findItem(child, name)) return found;
            return nullptr;
        };
        QTRY_VERIFY(findItem(hub.window()->contentItem(), "hubPower"));
        QVERIFY(!hub.window()->findChild<QObject*>("hubEmpty")->property("visible").toBool());
        if (!output.isEmpty()) {
            QTest::qWait(700);
            QVERIFY(hub.window()->grabWindow().save(QString("%1/hub-%2-%3-light.png").arg(output, model, language)));
        }
        hub.close();
        QTRY_VERIFY(!hub.isVisible());
        const bool previousSmoothing = controller.iconAntialiasing();
        controller.setIconAntialiasing(false);
        QCOMPARE(QSettings("SonyBridge", "SonyDeviceCenter").value("iconAntialiasing").toBool(), false);
        controller.setIconAntialiasing(previousSmoothing);
        controller.setThemeMode("dark");
        QTest::qWait(20);
        QCOMPARE(window->color(), QColor("#0C0C0C"));
        controller.setAnimationsEnabled(false);
        QCOMPARE(QSettings("SonyBridge", "SonyDeviceCenter").value("animationsEnabled").toBool(), false);
        controller.setThemeMode("invalid");
        QCOMPARE(controller.themeMode(), QString("dark"));
        // Hub behaviour, with motion off so nothing here waits on a slide.
        hub.open(QRect(window->x() + 300, window->y() + 500, 24, 24));
        QTRY_VERIFY(hub.isVisible());
        if (!output.isEmpty()) {
            QTest::qWait(700);
            QVERIFY(hub.window()->grabWindow().save(QString("%1/hub-%2-%3-dark.png").arg(output, model, language)));
        }
        // Esc closes; key events only reach the card once the window holds
        // focus, which is also what a real Esc press implies.
        hub.window()->requestActivate();
        QTRY_VERIFY(hub.window()->isActive());
        QTest::keyClick(hub.window(), Qt::Key_Escape);
        QTRY_VERIFY(!hub.isVisible());
        // A tray click right after the hub closed on focus loss is the click
        // that closed it, so it must not reopen; a later one does.
        hub.toggle();
        QTest::qWait(50);
        QVERIFY(!hub.isVisible());
        QTest::qWait(400);
        hub.toggle();
        QTRY_VERIFY(hub.isVisible());
        hub.toggle();
        QTRY_VERIFY(!hub.isVisible());
        // Rows follow the source without the list being rebuilt.
        peripheralSource.update("F4:73:35:AA:10:03", [](Peripheral& p) { p.connected = true; });
        QTRY_COMPARE(hubList->property("count").toInt(), peripherals.rowCount());
        // The Settings card is there, and the per-device pin appears in the
        // rows only once the tray is in per-device mode.
        QVERIFY(window->findChild<QObject*>("hubCard"));
        QVERIFY(window->findChild<QObject*>("hubShowSystemSwitch"));
        auto* pinItem = findItem(hub.window()->contentItem(), "hubPin");
        QVERIFY(pinItem);
        QVERIFY(!pinItem->isVisible());
        hubSettings.setTrayMode("perDevice");
        hubSettings.setInTray(peripherals.rows().first().peripheral.address, true);
        QTRY_VERIFY(pinItem->isVisible());
        if (!output.isEmpty()) {
            // The hub with a pinned row, and the Settings card in per-device
            // mode with its device list open.
            QTest::qWait(300);
            QVERIFY(hub.window()->grabWindow().save(QString("%1/hub-%2-%3-pinned.png").arg(output, model, language)));
            window->setProperty("navIndex", 6);
            auto* flick = window->findChild<QQuickItem*>("settingsFlick");
            auto* card = window->findChild<QQuickItem*>("hubCard");
            auto* content = flick ? flick->property("contentItem").value<QQuickItem*>() : nullptr;
            QVERIFY(flick && card && content);
            QTest::qWait(300);
            const qreal max = flick->property("contentHeight").toReal() - flick->height();
            flick->setProperty("contentY", (std::clamp)(card->mapToItem(content, QPointF(0, 0)).y() - 16, 0.0, (std::max)(0.0, max)));
            QTest::qWait(400);
            QVERIFY(window->grabWindow().save(QString("%1/settings-hub-%2-%3-%4.png").arg(output, model, language).arg(size.width())));
        }
        hubSettings.setTrayMode("single");
        // The footer opens the main window on the requested page.
        QSignalSpy mainRequests(&hub, &HubWindow::mainWindowRequested);
        QVERIFY(hub.window()->findChild<QObject*>("hubSettings"));
        QVERIFY(QMetaObject::invokeMethod(hub.window(), "mainWindowRequested", Q_ARG(int, 6)));
        QCOMPARE(mainRequests.count(), 1);
        QCOMPARE(mainRequests.first().first().toInt(), 6);
        // A tap on the quick controls stays in the hub: the mode changes and
        // the main window is not asked for. A tap on the row itself is.
        hub.open(QRect(window->x() + 300, window->y() + 500, 24, 24));
        QTRY_VERIFY(hub.isVisible());
        auto* offSegment = findItem(hub.window()->contentItem(), "hubMode-off");
        QVERIFY(offSegment);
        auto centre = [](QQuickItem* item) { return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint(); };
        QTest::mouseClick(hub.window(), Qt::LeftButton, Qt::NoModifier, centre(offSegment));
        QTRY_COMPARE_WITH_TIMEOUT(controller.noiseControlMode(), QString("off"), 5000);
        QCOMPARE(mainRequests.count(), 1);
        // Focus may have wandered back to the main window meanwhile (which
        // closes the hub, as designed); the row tap needs it up.
        if (!hub.isVisible()) hub.open(QRect(window->x() + 300, window->y() + 500, 24, 24));
        QTRY_VERIFY(hub.isVisible());
        auto* firstRowBackground = findItem(hub.window()->contentItem(), "hubRowBg");
        QVERIFY(firstRowBackground);
        QTest::mouseClick(hub.window(), Qt::LeftButton, Qt::NoModifier, firstRowBackground->mapToScene(QPointF(120, 28)).toPoint());
        QTRY_COMPARE(mainRequests.count(), 2);
        QCOMPARE(mainRequests.last().first().toInt(), 0);
        hub.close();
        QTRY_VERIFY(!hub.isVisible());
        controller.setAnimationsEnabled(previousAnimations);
        for (int page = 0; page < 7; ++page) {
            QVERIFY(window->setProperty("navIndex", page));
            const auto screenshotDirectory = qEnvironmentVariable("SONY_UI_SCREENSHOTS");
            QTest::qWait(screenshotDirectory.isEmpty() ? 30 : 400);
            if (!screenshotDirectory.isEmpty()) {
                QDir().mkpath(screenshotDirectory);
                const auto path = QString("%1/%2-%3-%4-page%5.png")
                    .arg(screenshotDirectory, model, language).arg(size.width()).arg(page);
                QVERIFY(window->grabWindow().save(path));
            }
        }
        // Each fact once: the title bar names the app, the sidebar foot
        // carries the connection state (in words unless the sidebar is
        // folded), and power off is at the sidebar foot and in the panel.
        QCOMPARE(window->title(), QString("Sony Device Center"));
        const bool folded = window->property("compact").toBool();
        QCOMPARE(folded, size.width() < 760);
        auto* sidebarState = window->findChild<QQuickItem*>("sidebarConnectionState");
        QVERIFY(sidebarState);
        QCOMPARE(sidebarState->isVisible(), !folded);
        QVERIFY(sidebarState->property("text").toString() == controller.t("connected")
                || sidebarState->property("text").toString() == controller.t("charging"));
        auto* sidebarPower = window->findChild<QQuickItem*>("sidebarPowerOff");
        auto* advancedButton = window->findChild<QQuickItem*>("advancedButton");
        auto* panelPower = window->findChild<QQuickItem*>("advancedPowerOff");
        QVERIFY(sidebarPower && advancedButton && panelPower);
        QVERIFY(sidebarPower->isEnabled() && panelPower->isEnabled());
        // The header row fits the window: the gear, its last item, ends
        // inside the page margin even at the minimum size.
        window->setProperty("navIndex", 0);
        QTest::qWait(30);
        QVERIFY2(advancedButton->mapToScene(QPointF(advancedButton->width(), 0)).x() <= window->width() - 20,
                 qPrintable(QString("header overflows: the gear ends at %1 of %2")
                     .arg(advancedButton->mapToScene(QPointF(advancedButton->width(), 0)).x()).arg(window->width())));
        auto* headerBattery = window->findChild<QQuickItem*>("headerBatteryDots");
        QVERIFY(headerBattery);
        QVERIFY(headerBattery->property("text").toString().endsWith("%"));
        // The Overview: the ring follows the mode, and a tap on a mode sets
        // it. Before any discharge is measured, the time left is the rated one.
        auto* ring = window->findChild<QQuickItem*>("modeRing");
        QVERIFY(ring);
        QTRY_COMPARE(ring->property("shown").toString(), controller.noiseControlMode());
        // (The hub test above left it off.)
        auto* ancButton = findItem(window->contentItem(), "modeButton_cancelling");
        QVERIFY(ancButton && ancButton->isVisible());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          ancButton->mapToScene(QPointF(ancButton->width() / 2, ancButton->height() / 2)).toPoint());
        QTRY_COMPARE_WITH_TIMEOUT(controller.noiseControlMode(), QString("cancelling"), 5000);
        QTRY_COMPARE(ring->property("shown").toString(), QString("cancelling"));
        QVERIFY(ancButton->property("current").toBool());
        auto* timeLeft = window->findChild<QObject*>("timeLeftDots");
        QVERIFY(timeLeft);
        if (controller.batteryMinutesLeft() >= 0) QVERIFY(timeLeft->property("text").toString().contains(":"));
        // The gear slides the panel in over the page; Esc takes it away.
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          advancedButton->mapToScene(QPointF(advancedButton->width() / 2, advancedButton->height() / 2)).toPoint());
        QTRY_VERIFY(window->property("advancedOpen").toBool());
        auto* panel = window->findChild<QQuickItem*>("advancedPanel");
        QVERIFY(panel);
        QTRY_VERIFY(panel->mapToScene(QPointF(panel->width(), 0)).x() <= window->width() + 0.5);
        QCOMPARE(window->findChild<QObject*>("advancedCodec")->property("value").toString(),
                 controller.codec() == "Unknown" ? QString::fromUtf8("—") : controller.codec());
        if (!output.isEmpty()) {
            QTest::qWait(500);
            QVERIFY(window->grabWindow().save(QString("%1/advanced-%2-%3-%4.png").arg(output, model, language).arg(size.width())));
        }
        // A row opens its page and closes the panel, and the tap goes no
        // further: at 980 px the Battery row lies over the Off tile, which
        // used to switch noise control off too.
        auto* batteryRow = window->findChild<QQuickItem*>("advancedBatteryRow");
        QVERIFY(batteryRow);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          batteryRow->mapToScene(QPointF(batteryRow->width() / 2, batteryRow->height() / 2)).toPoint());
        QTRY_COMPARE(window->property("navIndex").toInt(), 5);
        QVERIFY(!window->property("advancedOpen").toBool());
        QTest::qWait(300);
        QCOMPARE(controller.noiseControlMode(), QString("cancelling"));
        window->setProperty("navIndex", 0);
        window->setProperty("advancedOpen", true);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!window->property("advancedOpen").toBool());
        window->setProperty("navIndex", 6); // the About card checks below need Settings shown
        // The regular-font option swaps every dot display for the body
        // face, persists, and switches back.
        const bool previousPlainFont = controller.plainFont();
        auto* nameDots = window->findChild<QQuickItem*>("deviceNameDots");
        QVERIFY(nameDots);
        controller.setPlainFont(true);
        QCOMPARE(QSettings("SonyBridge", "SonyDeviceCenter").value("plainFont").toBool(), true);
        QTRY_VERIFY(nameDots->property("plain").toBool());
        QVERIFY(nameDots->implicitHeight() > 0);
        QCOMPARE(window->findChild<QObject*>("plainFontSwitch")->property("checked").toBool(), true);
        if (!output.isEmpty()) {
            for (int page : {0, 2, 5}) {
                window->setProperty("navIndex", page);
                QTest::qWait(400);
                QVERIFY(window->grabWindow().save(QString("%1/plain-%2-%3-%4-page%5.png")
                    .arg(output, model, language).arg(size.width()).arg(page)));
            }
        }
        controller.setPlainFont(false);
        QTRY_VERIFY(!nameDots->property("plain").toBool());
        controller.setPlainFont(previousPlainFont);
        // The About card follows the checker: idle, then the release with
        // both actions once the (canned) reply is in.
        auto* updateStatus = window->findChild<QObject*>("updateStatus");
        auto* download = window->findChild<QObject*>("updateDownload");
        auto* releasePage = window->findChild<QObject*>("updateReleasePage");
        auto* checkNow = window->findChild<QObject*>("updateCheckNow");
        QVERIFY(updateStatus && download && releasePage && checkNow);
        QCOMPARE(updateStatus->property("text").toString(), controller.t("update_idle"));
        QVERIFY(!download->property("visible").toBool());
        QVERIFY(checkNow->property("visible").toBool());
        updates.check();
        QTRY_COMPARE(updates.state(), QString("available"));
        QTest::qWait(20);
        QCOMPARE(updateStatus->property("text").toString(), controller.t("update_available").arg("9.9.9"));
        QVERIFY(download->property("visible").toBool());
        QVERIFY(releasePage->property("visible").toBool());
        QVERIFY(!checkNow->property("visible").toBool());
        const auto* smoothingSwitch = window->findChild<QObject*>("iconSmoothingSwitch");
        QVERIFY(smoothingSwitch);
        QCOMPARE(smoothingSwitch->property("checked").toBool(), controller.iconAntialiasing());
        const auto* animationsSwitch = window->findChild<QObject*>("animationsSwitch");
        QVERIFY(animationsSwitch);
        QCOMPARE(animationsSwitch->property("checked").toBool(), controller.animationsEnabled());
        // Typing into a dot-matrix value sends the clamped number to the device.
        if (controller.hasClearBass()) {
            auto* bassValue = window->findChild<QObject*>("clearBassValue");
            QVERIFY(bassValue);
            QVERIFY(QMetaObject::invokeMethod(bassValue, "apply", Q_ARG(QVariant, QVariant("37"))));
            QTRY_COMPARE_WITH_TIMEOUT(controller.clearBass(), 10, 3000);
            QVERIFY(QMetaObject::invokeMethod(bassValue, "apply", Q_ARG(QVariant, QVariant("-3"))));
            QTRY_COMPARE_WITH_TIMEOUT(controller.clearBass(), -3, 3000);
            QTRY_COMPARE_WITH_TIMEOUT(bassValue->property("value").toInt(), -3, 3000);
        }
        // Last, since the simulated set stays off: power off from the
        // panel closes it, and both buttons grey out.
        window->setProperty("navIndex", 0);
        window->setProperty("advancedOpen", true);
        QTRY_VERIFY(panelPower->isVisible());
        QTest::qWait(500); // the slide, if animations are on
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          panelPower->mapToScene(QPointF(panelPower->width() / 2, panelPower->height() / 2)).toPoint());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isConnected(), 5000);
        QVERIFY(!window->property("advancedOpen").toBool());
        QTRY_VERIFY(!panelPower->isEnabled() && !sidebarPower->isEnabled());
        QVERIFY(sidebarState->property("text").toString() != controller.t("connected"));
        controller.setThemeMode(previousTheme);
        controller.setLanguage(previousLanguage);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join("\n")));
    }
    void startupDoesNotBlockGui() {
        auto service = std::make_shared<SlowService>();
        QElapsedTimer elapsed; elapsed.start();
        DeviceCenterController controller(nullptr, service);
        QVERIFY(elapsed.elapsed() < 100);
        QVERIFY(controller.busy());
        bool guiTick = false;
        QTimer::singleShot(10, [&] { guiTick = true; });
        QTRY_VERIFY_WITH_TIMEOUT(guiTick, 100);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QCOMPARE(controller.batteryLevel(), -1);
        QCOMPARE(controller.noiseControlMode(), QString("unknown"));
        QCOMPARE(controller.codec(), QString("Unknown"));
    }
    void earbudsExposePerSideAndCaseBattery() {
        auto simulated = core::createSimulatedDevice("WF-1000XM5");
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QVERIFY(controller.hasDualBattery());
        QCOMPARE(controller.batteryLeft(), 81);
        QCOMPARE(controller.batteryRight(), 79);
        QCOMPARE(controller.batteryCase(), 64);
        QCOMPARE(controller.batteryLevel(), 79); // the weaker side drives the ring
    }
    void overEarHasNoDualBattery() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QVERIFY(!controller.hasDualBattery());
        QCOMPARE(controller.batteryLevel(), 87);
        QCOMPARE(controller.batteryCase(), -1);
    }
    void rapidSliderValuesCoalesceToTheLastOne() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        simulated.transport->clearSent();
        for (int level = 5; level <= 15; ++level) controller.setAmbient(level, false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && controller.ambientLevel() == 15, 5000);
        int noiseWrites = 0; int lastLevel = -1;
        for (const auto& raw : simulated.transport->sentFrames()) {
            const auto frame = protocol::FrameCodec::decode(raw);
            if (frame.payload.size() >= 7 && frame.payload[0] == 0x68) { ++noiseWrites; lastLevel = frame.payload[6]; }
        }
        QCOMPARE(lastLevel, 15);
        QVERIFY2(noiseWrites <= 3, qPrintable(QString("expected the drag to coalesce, got %1 writes").arg(noiseWrites)));
    }
    void ambientLevelSurvivesNoiseCancelling() {
        // Outside ambient mode the protocol reports level 0. The controller
        // must keep the last real level so "back to ambient" restores it
        // instead of asking for 0 (rejected) or 1 (wrong).
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        // The level is a persisted user setting; put it back afterwards.
        const auto previousLevel = QSettings("SonyBridge", "SonyDeviceCenter").value("ambientLevel", 10);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.noiseControlMode(), QString("cancelling"));
        auto settle = [&] { QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000); QVERIFY2(controller.lastError().isEmpty(), qPrintable(controller.lastError())); };
        controller.setAmbient(14, false); settle();
        QCOMPARE(controller.noiseControlMode(), QString("ambient"));
        QCOMPARE(controller.ambientLevel(), 14);
        controller.setAnc(true); settle();
        QCOMPARE(controller.noiseControlMode(), QString("cancelling"));
        QVERIFY2(controller.ambientLevel() == 14, "remembered while the device reports 0");
        controller.setAmbient(controller.ambientLevel(), false); settle();
        QCOMPARE(controller.noiseControlMode(), QString("ambient"));
        QCOMPARE(controller.ambientLevel(), 14);
        // Persisted, so an app restart starts from it too.
        QSettings settings("SonyBridge", "SonyDeviceCenter");
        QCOMPARE(settings.value("ambientLevel").toInt(), 14);
        settings.setValue("ambientLevel", previousLevel);
    }
    void trayIconReflectsBatteryAndConnection() {
        // Rendering is pure: no tray needed, so it runs headless too.
        auto pixel = [](const QIcon& icon, int x, int y) { return icon.pixmap(64, 64).toImage().pixelColor(x, y); };
        const auto full = TrayController::renderIcon(87, false, true);
        const auto low = TrayController::renderIcon(10, false, true);
        const auto gone = TrayController::renderIcon(-1, false, false);
        QVERIFY(!full.isNull());
        // Top of the ring at 12 o'clock is inside the filled arc for any level > 0.
        QCOMPARE(pixel(full, 32, 3).name(), QColor("#2DD4A7").name());
        QCOMPARE(pixel(low, 32, 3).name(), QColor("#FF5A5F").name());
        QCOMPARE(pixel(gone, 32, 3).name(), QColor("#3A3D48").name());
        QCOMPARE(pixel(TrayController::renderIcon(50, true, true), 32, 3).name(), QColor("#7C8CFF").name());
    }
    void hubShowsAnEmptyStateWithoutDevices() {
        // Nothing paired, nothing connected: the hub keeps its three-row
        // height and says so instead of showing a blank list.
        auto service = std::make_shared<WakeCountingService>();
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QQmlApplicationEngine engine;
        TrayController tray(controller);
        HotkeyManager hotkeys(controller, tray, nullptr, "hotkeys-test");
        QTemporaryDir libraryDir;
        EqualizerLibrary eqLibrary(controller, libraryDir.path());
        UpdateChecker updates(SONY_DEVICE_CENTER_VERSION, new test::FakeReleaseFetcher);
        FakePeripheralSource peripheralSource;
        PeripheralModel peripherals(controller, peripheralSource);
        QTemporaryDir hubSettingsDir;
        HubSettings hubSettings(hubSettingsDir.path() + "/hub.ini");
        engine.rootContext()->setContextProperty("hubSettings", &hubSettings);
        engine.rootContext()->setContextProperty("controller", &controller);
        engine.rootContext()->setContextProperty("hotkeys", &hotkeys);
        engine.rootContext()->setContextProperty("eqLibrary", &eqLibrary);
        engine.rootContext()->setContextProperty("updates", &updates);
        engine.rootContext()->setContextProperty("peripherals", &peripherals);
        engine.rootContext()->setContextProperty("trayAvailable", false);
        engine.rootContext()->setContextProperty("startHidden", true);
        engine.load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        HubWindow hub(engine, engine.rootObjects().first());
        QVERIFY(hub.isReady());
        QCOMPARE(peripherals.rowCount(), 0);
        hub.open(QRect(100, 100, 24, 24));
        QTRY_VERIFY(hub.isVisible());
        auto* empty = hub.window()->findChild<QObject*>("hubEmpty");
        QVERIFY(empty && empty->property("visible").toBool());
        QCOMPARE(hub.window()->findChild<QObject*>("hubList")->property("height").toInt(), 3 * 56);
        const auto output = qEnvironmentVariable("SONY_UI_SCREENSHOTS");
        if (!output.isEmpty()) {
            QTest::qWait(600);
            QVERIFY(hub.window()->grabWindow().save(QString("%1/hub-empty.png").arg(output)));
        }
        // A device arriving replaces the empty state in place.
        peripheralSource.setPeripherals(FakePeripheralSource::simulatedSet());
        QTRY_VERIFY(!empty->property("visible").toBool());
        QCOMPARE(hub.window()->findChild<QObject*>("hubList")->property("count").toInt(), 3);
        hub.close();
        QTRY_VERIFY(!hub.isVisible());
    }
    void hubSettingsPersistAndValidate() {
        QTemporaryDir dir;
        const auto ini = dir.path() + "/hub.ini";
        {
            HubSettings settings(ini);
            QCOMPARE(settings.showSystemDevices(), true);
            QCOMPARE(settings.pollIntervalSeconds(), 30);
            QCOMPARE(settings.trayClickAction(), QString("hub"));
            QCOMPARE(settings.trayMode(), QString("single"));
            QSignalSpy changed(&settings, &HubSettings::changed);
            settings.setShowSystemDevices(false);
            settings.setPollIntervalSeconds(5);        // clamped to 15
            settings.setTrayClickAction("nonsense");   // ignored
            settings.setTrayClickAction("window");
            settings.setTrayMode("perDevice");
            settings.setInTray("f4:73:35:aa:10:01", true);
            settings.setInTray("F4:73:35:AA:10:01", true);  // same device, no change
            QCOMPARE(changed.count(), 5);
            QCOMPARE(settings.pollIntervalSeconds(), 15);
            QVERIFY(settings.isInTray("F4-73-35-AA-10-01"));
        }
        HubSettings again(ini);
        QCOMPARE(again.showSystemDevices(), false);
        QCOMPARE(again.pollIntervalSeconds(), 15);
        QCOMPARE(again.trayClickAction(), QString("window"));
        QCOMPARE(again.trayMode(), QString("perDevice"));
        QCOMPARE(again.trayDevices(), QStringList{"F4:73:35:AA:10:01"});
        again.setInTray("F4:73:35:AA:10:01", false);
        QVERIFY(again.trayDevices().isEmpty());
    }
    void trayIconsFollowHubSettings() {
        // Rendering first: badge disc in the lower right corner on top of
        // the usual ring and number, for every class.
        for (auto kind : {PeripheralKind::Mouse, PeripheralKind::Keyboard, PeripheralKind::Gamepad, PeripheralKind::Headphones,
                          PeripheralKind::Earbuds, PeripheralKind::Other}) {
            const auto image = TrayController::renderDeviceIcon(50, true, kind).pixmap(64, 64).toImage();
            QVERIFY(!image.isNull());
            QCOMPARE(image.pixelColor(40, 54).name(), QColor("#3A3D48").name());  // the badge disc, clear of the glyph
            QCOMPARE(image.pixelColor(32, 3).name(), QColor("#2DD4A7").name());   // the ring is still there
        }
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        FakePeripheralSource source;
        source.setPeripherals(FakePeripheralSource::simulatedSet());
        PeripheralModel peripherals(controller, source);
        QTemporaryDir dir;
        HubSettings settings(dir.path() + "/hub.ini");
        TrayController tray(controller);
        if (!tray.isAvailable()) QSKIP("no system tray on this platform");
        tray.setHubSettings(&settings);
        tray.setPeripherals(&peripherals);
        QCOMPARE(tray.deviceIconCount(), 0);
        // Chosen devices count only in per-device mode.
        settings.setInTray("F4:73:35:AA:10:01", true);
        QCOMPARE(tray.deviceIconCount(), 0);
        settings.setTrayMode("perDevice");
        QCOMPARE(tray.deviceIconCount(), 1);
        settings.setInTray("F4:73:35:AA:10:02", true);
        QCOMPARE(tray.deviceIconCount(), 2);
        // An unpaired device loses its icon; back in the list it returns.
        auto rest = source.peripherals();
        rest.removeIf([](const Peripheral& p) { return p.kind == PeripheralKind::Keyboard; });
        source.setPeripherals(rest);
        QCOMPARE(tray.deviceIconCount(), 1);
        source.setPeripherals(FakePeripheralSource::simulatedSet());
        QCOMPARE(tray.deviceIconCount(), 2);
        settings.setTrayMode("single");
        QCOMPARE(tray.deviceIconCount(), 0);
    }
    void hubPlacementStaysOnScreen() {
        const QSize hub(360, 400);
        auto fits = [&hub](const QPoint& at, const QRect& available) {
            return available.contains(QRect(at, hub).adjusted(-7, -7, 7, 7));
        };
        // Bottom-right tray icon (a 40 px taskbar below the available area):
        // above the icon, pulled in from the right edge.
        QRect available(0, 0, 1920, 1040);
        auto at = HubWindow::placeNear(QRect(1880, 1045, 24, 24), hub, available);
        QVERIFY(fits(at, available));
        QVERIFY(at.y() + hub.height() < 1045);
        QCOMPARE(at.x(), 1919 - 360 - 8);
        // Top taskbar: below the icon, centred on it.
        available = QRect(0, 40, 1920, 1040);
        at = HubWindow::placeNear(QRect(1000, 8, 24, 24), hub, available);
        QVERIFY(fits(at, available));
        QCOMPARE(at.x(), 1011 - 180);  // centre of a 24 px icon at x=1000
        QVERIFY(at.y() >= 40 + 8);
        // Left taskbar: to the right of the icon.
        available = QRect(60, 0, 1860, 1080);
        at = HubWindow::placeNear(QRect(18, 900, 24, 24), hub, available);
        QVERIFY(fits(at, available));
        QCOMPARE(at.x(), 60 + 8);
        // The cursor in the very corner (no icon rectangle) still fits.
        available = QRect(0, 0, 1920, 1040);
        at = HubWindow::placeNear(QRect(1919, 1039, 1, 1), hub, available);
        QVERIFY(fits(at, available));
    }
    void notificationsFireOnceAtEachEdge() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        controller.setNotifyCharged(true);
        controller.setLowBatteryThreshold(20);
        TrayController tray(controller);
        NotificationController notifications(controller, tray);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && controller.batteryLevel() == 87, 5000);
        // Starting next to connected headphones is a baseline, not an event.
        QCOMPARE(notifications.messageCount(), 0);

        auto settle = [&](int level) { QTRY_COMPARE_WITH_TIMEOUT(controller.batteryLevel(), level, 3000); QTest::qWait(50); };
        simulated.transport->setBattery(18, false); settle(18);
        QCOMPARE(notifications.messageCount(), 1);
        QVERIFY(notifications.lastMessage().contains("18"));
        simulated.transport->setBattery(17, false); settle(17);
        QVERIFY2(notifications.messageCount() == 1, "still low: must not repeat");
        simulated.transport->setBattery(9, false); settle(9);
        QVERIFY2(notifications.messageCount() == 2, "critical step announces again");
        QVERIFY(notifications.lastMessage().contains("9"));
        simulated.transport->setBattery(100, true); settle(100);
        QCOMPARE(notifications.messageCount(), 3);
        QVERIFY(notifications.lastMessage().contains(controller.t("notify_charged")));
        QTest::qWait(1200);
        QVERIFY2(notifications.messageCount() == 3, "charged: polling must not repeat it");

        controller.setNotifyConnection(true);
        service->activeDevice()->powerOff();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isConnected(), 5000);
        QTest::qWait(50);
        QCOMPARE(notifications.messageCount(), 4);
        QVERIFY(notifications.lastMessage().contains(controller.t("notify_disconnected")));
    }
    void updateToastHonoursTheSetting() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        DeviceCenterController controller(nullptr, service);
        TrayController tray(controller);
        NotificationController notifications(controller, tray);
        const bool previous = controller.notifyUpdates();
        const bool previousCheck = controller.checkUpdatesOnStart();

        auto* fetcher = new test::FakeReleaseFetcher;
        fetcher->body = test::FakeReleaseFetcher::release("v9.9.9");
        UpdateChecker updates("0.2.1", fetcher);
        connect(&updates, &UpdateChecker::updateAvailable, &notifications, &NotificationController::announceUpdate);

        controller.setNotifyUpdates(false);
        QCOMPARE(QSettings("SonyBridge", "SonyDeviceCenter").value("notifyUpdates").toBool(), false);
        updates.check();
        QTRY_COMPARE(updates.state(), QString("available"));
        QVERIFY2(notifications.messageCount() == 0, "switched off: the card shows it, no toast");

        controller.setNotifyUpdates(true);
        auto* again = new test::FakeReleaseFetcher;
        again->body = test::FakeReleaseFetcher::release("v9.9.9");
        UpdateChecker fresh("0.2.1", again);
        connect(&fresh, &UpdateChecker::updateAvailable, &notifications, &NotificationController::announceUpdate);
        fresh.check();
        QTRY_COMPARE(fresh.state(), QString("available"));
        QCOMPARE(notifications.messageCount(), 1);
        QVERIFY(notifications.lastMessage().contains("9.9.9"));
        QVERIFY(notifications.lastMessage().contains(controller.t("notify_update_body")));

        controller.setCheckUpdatesOnStart(false);
        QCOMPARE(QSettings("SonyBridge", "SonyDeviceCenter").value("checkUpdatesOnStart").toBool(), false);
        controller.setCheckUpdatesOnStart(previousCheck);
        controller.setNotifyUpdates(previous);
    }
    void batteryLogFollowsTheSimulatedDevice() {
        QTemporaryDir dir;
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service, dir.path());
        QSignalSpy history(&controller, &DeviceCenterController::batteryHistoryChanged);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && controller.batteryLevel() == 87, 5000);
        auto& log = controller.batteryHistory();
        QCOMPARE(log.device(), QString::fromStdString(simulated.address));
        // Polling twice a second adds nothing while the level holds.
        QTest::qWait(1200);
        QCOMPARE(log.samples().size(), 1);
        QCOMPARE(log.samples()[0].event, BatteryHistory::Event::Connected);
        QCOMPARE(log.samples()[0].level, 87);
        // Nothing measured yet: the WH-1000XM5's rating stands in, 30 h
        // with noise cancelling on, so 87% is 26 h 6 min.
        QVERIFY(controller.batteryEstimateRated());
        QCOMPARE(controller.batteryMinutesLeft(), 87 * 30 * 60 / 100);
        QCOMPARE(controller.batteryDischargeRate(), 0.0);

        simulated.transport->setBattery(86, false);
        QTRY_COMPARE_WITH_TIMEOUT(log.samples().size(), 2, 3000);
        QCOMPARE(log.samples()[1].level, 86);
        QVERIFY2(controller.batteryEstimateRated(), "seconds of data are not a measurement");
        QCOMPARE(controller.batteryMinutesLeft(), 86 * 30 * 60 / 100);
        // Without noise processing the rating is 40 h.
        controller.setNoiseControlOff();
        QTRY_COMPARE_WITH_TIMEOUT(controller.noiseControlMode(), QString("off"), 3000);
        QCOMPARE(controller.batteryMinutesLeft(), 86 * 40 * 60 / 100);
        simulated.transport->setBattery(86, true);
        QTRY_COMPARE_WITH_TIMEOUT(log.samples().size(), 3, 3000);
        QVERIFY(log.samples()[2].charging);
        QCOMPARE(controller.batteryDischargeRate(), 0.0);
        QCOMPARE(controller.batteryMinutesLeft(), -1);
        QVERIFY(!controller.batteryEstimateRated());
        QVERIFY(history.count() >= 3);
        QVERIFY(QFile::exists(dir.path() + "/battery-history/CC-98-8B-00-11-22.json"));

        service->activeDevice()->powerOff();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isConnected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(log.samples().size(), 4, 3000);
        QCOMPARE(log.samples()[3].event, BatteryHistory::Event::Disconnected);
    }
    void durationsAreLocalised() {
        auto service = std::make_shared<SlowService>();
        DeviceCenterController controller(nullptr, service);
        // The language is a persisted user setting; put it back afterwards.
        const auto previous = controller.currentLanguage();
        controller.setLanguage("en");
        QCOMPARE(controller.formatDuration(320), QString("5 h 20 min"));
        QCOMPARE(controller.formatDuration(45), QString("45 min"));
        controller.setLanguage("ru");
        QCOMPARE(controller.formatDuration(320), QString::fromUtf8("5 ч 20 мин"));
        controller.setLanguage(previous);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
    }
    void failedActionPreservesConfirmedValue() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        QCOMPARE(controller.noiseControlMode(),QString("cancelling"));
        transport->simulateTimeoutOnSend();
        controller.setNoiseControlOff();
        QVERIFY(controller.busy());
        QCOMPARE(controller.noiseControlMode(),QString("cancelling"));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),3000);
        QVERIFY(!controller.lastError().isEmpty());
        QCOMPARE(controller.noiseControlMode(),QString("cancelling"));
    }
    void notificationsUpdateStateAndBands() {
        auto transport = std::make_shared<ReplyTransport>();
        auto service = std::make_shared<core::DeviceService>(transport);
        service->connect(transport::DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(),2000);
        QCOMPARE(controller.equalizerBands().size(),5);
        QCOMPARE(controller.equalizerBands()[4].toInt(),5);
        transport->notify({0x69,0x17,1,1,1,0,12});
        QTRY_COMPARE_WITH_TIMEOUT(controller.noiseControlMode(),QString("ambient"),1000);
        QCOMPARE(controller.ambientLevel(),12);
    }
    void hotkeyBindingsPersistAndParse() {
        // Parsing: one chord with a modifier, or a bare function key.
        QCOMPARE(HotkeyManager::normalize("Ctrl+Alt+N"), QString("Ctrl+Alt+N"));
        QCOMPARE(HotkeyManager::normalize("ctrl+alt+n"), QString("Ctrl+Alt+N"));
        QCOMPARE(HotkeyManager::normalize("  Shift+F5 "), QString("Shift+F5"));
        QCOMPARE(HotkeyManager::normalize("F9"), QString("F9"));
        QVERIFY2(HotkeyManager::normalize("N").isEmpty(), "a bare letter would swallow typing");
        QVERIFY(HotkeyManager::normalize("Ctrl").isEmpty());
        QVERIFY(HotkeyManager::normalize("Ctrl+A, Ctrl+B").isEmpty());
        QVERIFY(HotkeyManager::normalize("").isEmpty());
        QVERIFY(HotkeyManager::normalize("+++").isEmpty());
        QCOMPARE(HotkeyManager::sequenceFromKey(Qt::Key_N, Qt::ControlModifier | Qt::AltModifier), QString("Ctrl+Alt+N"));
        QCOMPARE(HotkeyManager::sequenceFromKey(Qt::Key_N, Qt::ControlModifier | Qt::KeypadModifier), QString("Ctrl+N"));
        QVERIFY2(HotkeyManager::sequenceFromKey(Qt::Key_Control, Qt::ControlModifier).isEmpty(), "modifier alone");
        QVERIFY(HotkeyManager::sequenceFromKey(Qt::Key_N, Qt::NoModifier).isEmpty());
        QCOMPARE(HotkeyManager::actionKey(HotkeyManager::Action::ShowWindow), QString("showWindow"));
        QVERIFY(HotkeyManager::actionFromKey("toggleSpeakToChat") == HotkeyManager::Action::ToggleSpeakToChat);
        QVERIFY(!HotkeyManager::actionFromKey("nope"));

        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        DeviceCenterController controller(nullptr, service);
        TrayController tray(controller);
        const QString group = "hotkeys-test";
        auto wipe = [&] { QSettings settings("SonyBridge", "SonyDeviceCenter"); settings.beginGroup(group); settings.remove(""); };
        wipe();
        {
            HotkeyManager hotkeys(controller, tray, nullptr, group);
            const auto bindings = hotkeys.bindings();
            QCOMPARE(bindings.size(), HotkeyManager::kActionCount);
            QCOMPARE(bindings[0].toMap()["action"].toString(), QString("toggleNoiseControl"));
            QCOMPARE(bindings[3].toMap()["action"].toString(), QString("showWindow"));
            for (const auto& entry : bindings) {
                QVERIFY2(!entry.toMap()["enabled"].toBool(), "off by default");
                QCOMPARE(entry.toMap()["status"].toString(), QString("off"));
            }
            QSignalSpy changed(&hotkeys, &HotkeyManager::bindingsChanged);
            hotkeys.setShortcut("toggleNoiseControl", "ctrl+alt+n");
            hotkeys.setEnabled("toggleNoiseControl", true);
            hotkeys.setShortcut("showWindow", "bogus");
            hotkeys.setEnabled("showWindow", true);
            QCOMPARE(changed.count(), 3);
            QCOMPARE(hotkeys.shortcut(HotkeyManager::Action::ToggleNoiseControl), QString("Ctrl+Alt+N"));
            QVERIFY2(hotkeys.shortcut(HotkeyManager::Action::ShowWindow).isEmpty(), "unusable text clears");
            QCOMPARE(hotkeys.status(HotkeyManager::Action::ShowWindow), HotkeyManager::Status::Off);
            const auto expected = HotkeyManager::supported() ? HotkeyManager::Status::Registered : HotkeyManager::Status::Unsupported;
            QCOMPARE(hotkeys.status(HotkeyManager::Action::ToggleNoiseControl), expected);
            if (HotkeyManager::supported()) {
                // The same combination twice: the OS refuses the second one.
                hotkeys.setShortcut("noiseControlOff", "Ctrl+Alt+N");
                hotkeys.setEnabled("noiseControlOff", true);
                QCOMPARE(hotkeys.status(HotkeyManager::Action::NoiseControlOff), HotkeyManager::Status::Conflict);
                QCOMPARE(hotkeys.bindings()[1].toMap()["status"].toString(), QString("conflict"));
                hotkeys.setShortcut("noiseControlOff", "Ctrl+Alt+O");
                QCOMPARE(hotkeys.status(HotkeyManager::Action::NoiseControlOff), HotkeyManager::Status::Registered);
                // Suspended for the capture field: nothing is held, status stays.
                hotkeys.suspend(true);
                QCOMPARE(hotkeys.status(HotkeyManager::Action::NoiseControlOff), HotkeyManager::Status::Registered);
                hotkeys.suspend(false);
            }
        }
        {
            // A fresh manager reads the same bindings back.
            HotkeyManager hotkeys(controller, tray, nullptr, group);
            QVERIFY(hotkeys.isEnabled(HotkeyManager::Action::ToggleNoiseControl));
            QCOMPARE(hotkeys.shortcut(HotkeyManager::Action::ToggleNoiseControl), QString("Ctrl+Alt+N"));
            QVERIFY(hotkeys.isEnabled(HotkeyManager::Action::ShowWindow));
            QVERIFY(hotkeys.shortcut(HotkeyManager::Action::ShowWindow).isEmpty());
            QCOMPARE(hotkeys.bindings()[0].toMap()["display"].toString(), HotkeyManager::displayText("Ctrl+Alt+N"));
        }
        wipe();
    }
    void hotkeysRouteActionsToController() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        const auto previousLevel = QSettings("SonyBridge", "SonyDeviceCenter").value("ambientLevel", 10);
        DeviceCenterController controller(nullptr, service);
        const bool previousNotify = controller.notifyHotkeys();
        controller.setNotifyHotkeys(true);
        TrayController tray(controller);
        HotkeyManager hotkeys(controller, tray, nullptr, "hotkeys-test");
        QSignalSpy shown(&hotkeys, &HotkeyManager::showWindowRequested);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.noiseControlMode(), QString("cancelling"));
        auto settle = [&] { QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000); QVERIFY2(controller.lastError().isEmpty(), qPrintable(controller.lastError())); };

        controller.setAmbient(14, false); settle();
        controller.setAnc(true); settle();
        using Action = HotkeyManager::Action;
        hotkeys.trigger(Action::ToggleNoiseControl); settle();
        QCOMPARE(controller.noiseControlMode(), QString("ambient"));
        QVERIFY2(controller.ambientLevel() == 14, "the remembered level, like the tray menu");
        QCOMPARE(hotkeys.lastFeedback(), controller.t("ambient_sound"));
        hotkeys.trigger(Action::ToggleNoiseControl); settle();
        QCOMPARE(controller.noiseControlMode(), QString("cancelling"));
        QCOMPARE(hotkeys.lastFeedback(), controller.t("noise_cancelling"));
        hotkeys.trigger(Action::NoiseControlOff); settle();
        QCOMPARE(controller.noiseControlMode(), QString("off"));
        QCOMPARE(hotkeys.lastFeedback(), controller.t("noise_control_off"));
        hotkeys.trigger(Action::ToggleNoiseControl); settle();
        QVERIFY2(controller.noiseControlMode() == "ambient", "off goes to ambient");

        const bool speak = controller.speakToChat();
        hotkeys.trigger(Action::ToggleSpeakToChat); settle();
        QCOMPARE(controller.speakToChat(), !speak);
        QVERIFY(hotkeys.lastFeedback().startsWith("Speak-to-Chat"));
        hotkeys.trigger(Action::ToggleSpeakToChat); settle();
        QCOMPARE(controller.speakToChat(), speak);

        hotkeys.trigger(Action::ShowWindow);
        QCOMPARE(shown.count(), 1);
#ifdef Q_OS_WIN
        // The real delivery path: a WM_HOTKEY thread message reaches the
        // native event filter through Qt's dispatcher. Posted by hand, so no
        // keystroke is injected into the session.
        hotkeys.setShortcut("showWindow", "Ctrl+Alt+F12");
        hotkeys.setEnabled("showWindow", true);
        QCOMPARE(hotkeys.status(Action::ShowWindow), HotkeyManager::Status::Registered);
        PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY,
                           HotkeyManager::kNativeIdBase + static_cast<int>(Action::ShowWindow), 0);
        QTRY_COMPARE_WITH_TIMEOUT(shown.count(), 2, 2000);
        hotkeys.setEnabled("showWindow", false);
        // Disabled: the same message is ignored.
        PostThreadMessageW(GetCurrentThreadId(), WM_HOTKEY,
                           HotkeyManager::kNativeIdBase + static_cast<int>(Action::ShowWindow), 0);
        QTest::qWait(100);
        QCOMPARE(shown.count(), 2);
        QSettings settings("SonyBridge", "SonyDeviceCenter"); settings.beginGroup("hotkeys-test"); settings.remove("");
#endif

        // Feedback follows the notification toggle.
        controller.setNotifyHotkeys(false);
        const auto before = hotkeys.lastFeedback();
        hotkeys.trigger(Action::NoiseControlOff); settle();
        QCOMPARE(hotkeys.lastFeedback(), before);

        controller.setNotifyHotkeys(previousNotify);
        QSettings("SonyBridge", "SonyDeviceCenter").setValue("ambientLevel", previousLevel);
    }
    void equalizerLibraryStoresAppliesAndImports() {
        auto simulated = core::createSimulatedDevice();
        auto service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        DeviceCenterController controller(nullptr, service);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        auto settle = [&] { QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000); QVERIFY2(controller.lastError().isEmpty(), qPrintable(controller.lastError())); };
        auto name = [](const EqualizerLibrary& library, int i) { return library.presets()[i].toMap()["name"].toString(); };
        const QVariantList warmCurve{4, 2, 0, -1, -3};

        QString warm, snap;
        {
            EqualizerLibrary library(controller, dir.path());
            QVERIFY(library.presets().isEmpty());
            QVERIFY(library.activeId().isEmpty());
            // Curves are validated: five bands, every gain within ±10.
            QVERIFY(library.save("Bad", 0, {1, 2, 3}).isEmpty());
            QVERIFY(library.save("Bad", 11, {0, 0, 0, 0, 0}).isEmpty());
            QVERIFY(library.save("Bad", 0, {0, 0, 0, 0, 12}).isEmpty());
            QCOMPARE(library.presets().size(), 0);

            QSignalSpy changed(&library, &EqualizerLibrary::presetsChanged);
            warm = library.save("Warm", 3, warmCurve);
            QVERIFY(!warm.isEmpty());
            // Names are unique (case-insensitively) and never empty.
            library.save("warm", 0, {0, 0, 0, 0, 0});
            QCOMPARE(name(library, 1), QString("warm (2)"));
            library.save("  ", 0, {1, 1, 1, 1, 1});
            QCOMPARE(name(library, 2), QString("Preset 3"));
            QCOMPARE(changed.count(), 3);

            // Applying fills the headphones' custom slot; the highlight
            // follows the device state, not the click.
            library.apply(warm); settle();
            QCOMPARE(controller.equalizerPreset(), EqualizerLibrary::kCustomPreset);
            QCOMPARE(controller.clearBass(), 3);
            QCOMPARE(controller.equalizerBands(), warmCurve);
            QCOMPARE(library.activeId(), warm);
            controller.setEqualizerPreset(0x16); settle();
            QVERIFY2(library.activeId().isEmpty(), "a built-in preset is nobody's curve");

            // saveCurrent captures what the device plays right now.
            controller.setEqualizerCustom(-2, {1, 0, -1, 0, 1}); settle();
            snap = library.saveCurrent("Snap");
            QCOMPARE(library.activeId(), snap);
            QVERIFY(library.rename(snap, "Warm"));
            QCOMPARE(name(library, 3), QString("Warm (3)"));  // "warm (2)" is taken too
            QVERIFY2(library.rename(warm, "Warm"), "keeping one's own name is fine");
            QCOMPARE(name(library, 0), QString("Warm"));
            QVERIFY(!library.rename("missing", "x"));

            // Overwrite with the current curve moves the highlight.
            QVERIFY(library.updateFromCurrent(warm));
            QCOMPARE(library.presets()[0].toMap()["clearBass"].toInt(), -2);
            QCOMPARE(library.activeId(), warm);

            // Export one and all, import both into a fresh library.
            const auto one = dir.filePath("one.json");
            const auto all = dir.filePath("all.json");
            QVERIFY(library.exportPreset(warm, one));
            QVERIFY(library.exportPreset("", all));
            QVERIFY(!library.exportPreset("missing", one));
            QTemporaryDir otherDir;
            EqualizerLibrary other(controller, otherDir.path());
            QCOMPARE(other.importFile(one), 1);
            QCOMPARE(name(other, 0), QString("Warm"));
            QCOMPARE(other.presets()[0].toMap()["bands"].toList(), QVariantList({1, 0, -1, 0, 1}));
            QCOMPARE(other.importFile(all), 4);
            QCOMPARE(name(other, 1), QString("Warm (2)"));
            QVERIFY2(other.presets()[1].toMap()["id"].toString() != warm, "imported presets get fresh ids");
            QVERIFY(other.lastError().isEmpty());
            const auto junk = dir.filePath("junk.json");
            { QFile f(junk); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("{\"presets\": [{\"name\": \"x\", \"bands\": [1, 2]}]}"); }
            QCOMPARE(other.importFile(junk), 0);
            QVERIFY(!other.lastError().isEmpty());
            QCOMPARE(other.importFile(dir.filePath("missing.json")), -1);

            QVERIFY(library.remove(snap));
            QVERIFY(!library.remove(snap));
            QCOMPARE(library.presets().size(), 3);
        }
        {
            // Persisted: the same directory reads back with ids intact.
            EqualizerLibrary library(controller, dir.path());
            QCOMPARE(library.presets().size(), 3);
            QCOMPARE(library.presets()[0].toMap()["id"].toString(), warm);
            QCOMPARE(name(library, 0), QString("Warm"));
            QCOMPARE(library.activeId(), warm);
            QVERIFY(QFile::exists(library.filePath()));
        }
    }
    void bluetoothWakeReachesTheService() {
        QCOMPARE(BluetoothWatcher::formatAddress(0xCC988B001122ULL), QString("CC:98:8B:00:11:22"));
        QCOMPARE(BluetoothWatcher::formatAddress(0), QString("00:00:00:00:00:00"));
        // With or without a radio the watcher must construct and tear down
        // cleanly; whether it is available depends on the machine.
        BluetoothWatcher watcher;
        QSignalSpy links(&watcher, &BluetoothWatcher::connectionChanged);
        QSignalSpy availability(&watcher, &BluetoothWatcher::availabilityChanged);
        const bool available = watcher.isAvailable();
        watcher.rescanRadios();
        QCOMPARE(availability.count(), 1);
        QCOMPARE(watcher.isAvailable(), available);
        QCOMPARE(links.count(), 0);

        auto service = std::make_shared<WakeCountingService>();
        DeviceCenterController controller(nullptr, service);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        // What main.cpp does when the watcher reports a link coming up.
        controller.wakeConnection();
        QTRY_COMPARE_WITH_TIMEOUT(service->wakes.load(), 1, 2000);
        controller.wakeConnection();
        controller.wakeConnection();
        QTRY_COMPARE_WITH_TIMEOUT(service->wakes.load(), 3, 2000);
    }
    void destructionDrainsWorkerAndCallbacks() {
        auto service = std::make_shared<SlowService>();
        auto controller = std::make_unique<DeviceCenterController>(nullptr,service);
        QTest::qWait(20);
        controller.reset();
        QCoreApplication::processEvents();
    }
};
// A full (widgets) application: the tray icon renderer paints with fonts.
QTEST_MAIN(DeviceControllerTests)
#include "DeviceControllerTests.moc"
