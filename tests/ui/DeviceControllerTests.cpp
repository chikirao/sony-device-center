#include <QtTest>
#include "DeviceCenterController.h"
#include "TrayController.h"
#include <QImage>
#include "sony/core/DeviceService.h"
#include "sony/core/SimulatedDevice.h"
#include "sony/protocol/FrameCodec.h"
#include "../support/ReplyTransport.h"
#include <chrono>
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
class DeviceControllerTests : public QObject {
    Q_OBJECT
private slots:
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
