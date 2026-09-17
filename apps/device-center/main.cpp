#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QDebug>
#include <QDir>
#include <QQuickItem>
#include <QQuickWindow>
#include <QDateTime>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QTimer>
#include <QWindow>
#include <algorithm>

#include "DeviceCenterController.h"
#include "TrayController.h"
#include "NotificationController.h"
#include "UpdateChecker.h"
#include "HotkeyManager.h"
#include "EqualizerLibrary.h"
#include "BluetoothWatcher.h"
#include "WindowsToast.h"
#include "sony/core/DeviceService.h"
#include "sony/core/SimulatedDevice.h"

namespace {
// --simulated-update <version>: what GitHub would say about a release of
// that version, with the usual asset names, so the Settings card and the
// toast can be looked at without a network or a real release.
class CannedReleaseFetcher : public sony::devicecenter::ReleaseFetcher {
public:
    explicit CannedReleaseFetcher(QString version) : _version(std::move(version)) {}
    void fetch(const QUrl&) override {
        const auto tag = "v" + _version;
        const auto base = "https://github.com/chikirao/sony-device-center/releases/";
        QStringList assets;
        for (const char* suffix : {"Linux.deb", "macOS.dmg", "win64.msi"})
            assets << QString(R"({"name":"sony-device-center-%1-%2","browser_download_url":"%3download/%4/sony-device-center-%1-%2"})")
                          .arg(_version, suffix, base, tag);
        const auto body = QString(R"({"tag_name":"%1","html_url":"%2tag/%1","assets":[%3]})").arg(tag, base, assets.join(',')).toUtf8();
        QTimer::singleShot(600, this, [this, body] { emit finished(200, body, {}); });
    }
private:
    QString _version;
};
}

int main(int argc, char *argv[]) {
    // QApplication rather than QGuiApplication: the tray icon and its menu
    // come from QtWidgets. The QML side is unaffected.
    // Screenshot mode must not depend on the window being visible on screen:
    // the threaded render loop only advances animations while exposed, so an
    // occluded or locked desktop would grab frozen first frames.
    if (!qEnvironmentVariable("SONY_UI_SCREENSHOTS").isEmpty()) qputenv("QSG_RENDER_LOOP", "basic");
    QApplication app(argc, argv);
    sony::devicecenter::WindowsToast::registerApplication();

    // Main.qml customises background/handle/indicator on its controls. The
    // native "Windows" and "macOS" styles Qt picks by default there refuse
    // that (one warning per control, and native-looking widgets), so pin the
    // one style that honours customisation on every platform.
    QQuickStyle::setStyle("Basic");
    app.setApplicationName("Sony Device Center");
    app.setOrganizationName("SonyBridge");

    // Manrope is the body face (Latin + Cyrillic); anything it lacks (kana)
    // falls through to the system font per glyph, which Qt handles itself.
    for (const auto* face : {"Regular", "Medium", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(QString(":/fonts/Manrope-%1.ttf").arg(face));
    QFont bodyFont("Manrope");
    bodyFont.setPixelSize(13);
    app.setFont(bodyFont);
    app.setApplicationVersion(SONY_DEVICE_CENTER_VERSION);

    // Wayland and the GNOME/KDE shells match a window to its .desktop entry by
    // this name; without it the taskbar falls back to a generic placeholder
    // even though the window icon below is set.
    QGuiApplication::setDesktopFileName("sony-device-center");

    // The raster form is used deliberately: QIcon can only read the SVG brand
    // asset when Qt's qsvg image plugin is deployed alongside the binary.
    app.setWindowIcon(QIcon(":/resources/brand/app-icon.png"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Desktop companion for Sony headphones and earbuds");
    parser.addHelpOption();
    parser.addVersionOption();
    // The daemon's simulator, but in-process: lets the UI be developed and
    // screenshotted without a headset, and on Windows, where sonyd cannot run.
    QCommandLineOption simulatedOption("simulated", "Drive the UI with a built-in simulated WH-1000XM5 instead of Bluetooth.");
    parser.addOption(simulatedOption);
    QCommandLineOption simulatedModelOption("simulated-model",
        "Model name for --simulated; WF-* and LinkBuds names simulate earbuds with left/right/case batteries.", "name");
    parser.addOption(simulatedModelOption);
    QCommandLineOption simulatedHistoryOption("simulated-history",
        "With --simulated: replace the battery log with a synthetic week of use so the chart and the estimate are populated.");
    parser.addOption(simulatedHistoryOption);
    QCommandLineOption simulatedUpdateOption("simulated-update",
        "Answer the update check with a canned release of the given version instead of asking GitHub.", "version");
    parser.addOption(simulatedUpdateOption);
    QCommandLineOption minimizedOption("minimized", "Start hidden in the system tray (used by autostart).");
    parser.addOption(minimizedOption);
    parser.process(app);

    std::shared_ptr<sony::core::IDeviceService> service;
    QString simulatedAddress;
    if (parser.isSet(simulatedOption) || parser.isSet(simulatedModelOption)) {
        auto simulated = sony::core::createSimulatedDevice(parser.value(simulatedModelOption).toStdString());
        simulatedAddress = QString::fromStdString(simulated.address);
        // Two more paired-but-idle sets so the Devices page has a list to
        // lay out. They share the one simulated transport: "connecting" to
        // either simply reconnects the same fake headset under that name.
        simulated.discovery->addDevice({.name = "WH-1000XM4", .address = sony::transport::DeviceAddress("CC:98:8B:00:11:33"),
                                        .paired = true, .connected = false});
        simulated.discovery->addDevice({.name = "LinkBuds S", .address = sony::transport::DeviceAddress("CC:98:8B:00:11:44"),
                                        .paired = true, .connected = false});
        auto simulatedService = std::make_shared<sony::core::DeviceService>(simulated.transport, simulated.discovery);
        simulatedService->startAutoConnect(simulated.address);
        service = std::move(simulatedService);
        // Drain the simulated battery 1% every 3 s so the tray, the ring and
        // the low-battery notification can be watched without waiting hours.
        auto* drain = new QTimer(&app);
        QObject::connect(drain, &QTimer::timeout, &app, [transport = simulated.transport, level = 87]() mutable {
            if (level > 0) transport->setBattery(--level, false);
        });
        drain->start(3000);
    }

    sony::devicecenter::DeviceCenterController controller(nullptr, std::move(service));
    if (parser.isSet(simulatedHistoryOption) && !simulatedAddress.isEmpty()) {
        // Seeded before the first snapshot arrives; the controller then finds
        // the log already open for the simulator's address.
        controller.batteryHistory().setDevice(simulatedAddress);
        controller.batteryHistory().seedDemoData(QDateTime::currentMSecsSinceEpoch());
    }
    sony::devicecenter::TrayController tray(controller);
    sony::devicecenter::NotificationController notifications(controller, tray);
    // Global shortcuts (Windows only); disabled until the user binds them.
    sony::devicecenter::HotkeyManager hotkeys(controller, tray);
    sony::devicecenter::EqualizerLibrary eqLibrary(controller);
    // One GET to the GitHub Releases API on launch (unless switched off) and
    // whenever Settings asks; nothing waits for it, and offline just fails
    // quietly into the Settings card.
    sony::devicecenter::ReleaseFetcher* fetcher = new sony::devicecenter::NetworkReleaseFetcher;
    if (parser.isSet(simulatedUpdateOption)) fetcher = new CannedReleaseFetcher(parser.value(simulatedUpdateOption));
    sony::devicecenter::UpdateChecker updates(SONY_DEVICE_CENTER_VERSION, fetcher);
    QObject::connect(&updates, &sony::devicecenter::UpdateChecker::updateAvailable, &notifications,
                     &sony::devicecenter::NotificationController::announceUpdate);
    // Screenshot runs stay off the network unless the reply is canned.
    const bool canned = parser.isSet(simulatedUpdateOption);
    if (controller.checkUpdatesOnStart() && (canned || qEnvironmentVariable("SONY_UI_SCREENSHOTS").isEmpty()))
        QTimer::singleShot(canned ? 0 : 1500, &updates, &sony::devicecenter::UpdateChecker::check);
    // Reconnect the moment the OS sees the headphones come up, instead of at
    // the next backed-off retry. Any device's link counts: the retry itself
    // is cheap and the watcher cannot tell Sony from a mouse.
    sony::devicecenter::BluetoothWatcher bluetoothWatcher;
    QObject::connect(&bluetoothWatcher, &sony::devicecenter::BluetoothWatcher::connectionChanged, &controller,
                     [&controller](const QString& address, bool connected) {
        qInfo().noquote() << "Bluetooth link" << (connected ? "up:" : "down:") << address;
        if (connected) controller.wakeConnection();
    });
    auto logWatcher = [&bluetoothWatcher] {
        qInfo() << (bluetoothWatcher.isAvailable() ? "Bluetooth link events on; reconnecting on arrival"
                                                   : "Bluetooth link events unavailable; relying on periodic retries");
    };
    QObject::connect(&bluetoothWatcher, &sony::devicecenter::BluetoothWatcher::availabilityChanged, &controller, logWatcher);
    logWatcher();
    // Without a tray there is nowhere to come back from, so a hidden start
    // and close-to-tray only make sense when the icon actually exists.
    const bool startHidden = parser.isSet(minimizedOption) && tray.isAvailable();
    app.setQuitOnLastWindowClosed(!tray.isAvailable());
    QObject::connect(&tray, &sony::devicecenter::TrayController::quitRequested, &app, &QCoreApplication::quit);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    engine.rootContext()->setContextProperty("hotkeys", &hotkeys);
    engine.rootContext()->setContextProperty("eqLibrary", &eqLibrary);
    engine.rootContext()->setContextProperty("updates", &updates);
    engine.rootContext()->setContextProperty("trayAvailable", tray.isAvailable());
    engine.rootContext()->setContextProperty("startHidden", startHidden);

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);
    if (!engine.rootObjects().isEmpty()) tray.setWindow(qobject_cast<QWindow*>(engine.rootObjects().first()));

    // SONY_UI_SCREENSHOTS=<dir>: walk every page, save a capture of each and
    // quit. Used to review the UI without driving the real mouse.
    const auto shotDir = qEnvironmentVariable("SONY_UI_SCREENSHOTS");
    auto* window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!shotDir.isEmpty() && window) {
        QDir().mkpath(shotDir);
        // SONY_UI_WINDOW=980x660 checks the layout at a given size (the
        // minimum is the interesting one).
        const auto size = qEnvironmentVariable("SONY_UI_WINDOW").split('x');
        if (size.size() == 2) window->resize(size[0].toInt(), size[1].toInt());
        auto* ticker = new QTimer(&app);
        int page = 0;
        QObject::connect(ticker, &QTimer::timeout, &app, [&, ticker, window]() mutable {
            if (page > 0 && page <= 7) window->grabWindow().save(QString("%1/page%2.png").arg(shotDir).arg(page - 1));
            // Settings is taller than the window: two more captures, scrolled
            // to the hotkeys card and then to the About/updates card.
            auto scrollTo = [window](const char* card) {
                auto* flick = window->findChild<QQuickItem*>("settingsFlick");
                auto* item = window->findChild<QQuickItem*>(card);
                auto* content = flick ? flick->property("contentItem").value<QQuickItem*>() : nullptr;
                if (flick && item && content) {
                    const qreal max = flick->property("contentHeight").toReal() - flick->height();
                    const qreal y = item->mapToItem(content, QPointF(0, 0)).y();
                    flick->setProperty("contentY", std::clamp(y - 16, 0.0, std::max(0.0, max)));
                }
            };
            if (page == 7) {
                scrollTo("hotkeysCard");
            } else if (page == 8) {
                window->grabWindow().save(QString("%1/page6-hotkeys.png").arg(shotDir));
                scrollTo("aboutCard");
            } else if (page == 9) {
                window->grabWindow().save(QString("%1/page6-about.png").arg(shotDir));
                ticker->stop(); app.quit(); return;
            }
            if (page <= 6) window->setProperty("navIndex", page);
            ++page;
        });
        ticker->start(1500);
    }

    return app.exec();
}
