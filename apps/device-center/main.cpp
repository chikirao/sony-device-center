#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QTimer>
#include <QWindow>

#include "DeviceCenterController.h"
#include "TrayController.h"
#include "NotificationController.h"
#include "WindowsToast.h"
#include "sony/core/DeviceService.h"
#include "sony/core/SimulatedDevice.h"

int main(int argc, char *argv[]) {
    // QApplication rather than QGuiApplication: the tray icon and its menu
    // come from QtWidgets. The QML side is unaffected.
    QApplication app(argc, argv);
    sony::devicecenter::WindowsToast::registerApplication();

    // Main.qml customises background/handle/indicator on its controls. The
    // native "Windows" and "macOS" styles Qt picks by default there refuse
    // that (one warning per control, and native-looking widgets), so pin the
    // one style that honours customisation on every platform.
    QQuickStyle::setStyle("Basic");
    app.setApplicationName("Sony Device Center");
    app.setOrganizationName("SonyBridge");
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
    QCommandLineOption minimizedOption("minimized", "Start hidden in the system tray (used by autostart).");
    parser.addOption(minimizedOption);
    parser.process(app);

    std::shared_ptr<sony::core::IDeviceService> service;
    if (parser.isSet(simulatedOption) || parser.isSet(simulatedModelOption)) {
        auto simulated = sony::core::createSimulatedDevice(parser.value(simulatedModelOption).toStdString());
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
    sony::devicecenter::TrayController tray(controller);
    sony::devicecenter::NotificationController notifications(controller, tray);
    // Without a tray there is nowhere to come back from, so a hidden start
    // and close-to-tray only make sense when the icon actually exists.
    const bool startHidden = parser.isSet(minimizedOption) && tray.isAvailable();
    app.setQuitOnLastWindowClosed(!tray.isAvailable());
    QObject::connect(&tray, &sony::devicecenter::TrayController::quitRequested, &app, &QCoreApplication::quit);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
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

    return app.exec();
}
