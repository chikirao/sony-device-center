#include <QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "DeviceCenterController.h"
#include "PeripheralModel.h"
#include "PeripheralSource.h"
#include "sony/core/DeviceService.h"
#include "sony/core/SimulatedDevice.h"

using namespace sony;
using namespace sony::devicecenter;

namespace {

// The simulator plus the two idle sets main() seeds for the Devices page.
struct Rig {
    core::SimulatedDevice simulated;
    std::shared_ptr<core::DeviceService> service;
    QTemporaryDir historyDir;
    std::unique_ptr<DeviceCenterController> controller;
    FakePeripheralSource source;
    std::unique_ptr<PeripheralModel> model;

    explicit Rig(const char* modelName = "WH-1000XM5", bool connect = true)
        : simulated(core::createSimulatedDevice(modelName)) {
        simulated.discovery->addDevice({.name = "WH-1000XM4", .address = transport::DeviceAddress("CC:98:8B:00:11:33"),
                                        .paired = true, .connected = false});
        simulated.discovery->addDevice({.name = "LinkBuds S", .address = transport::DeviceAddress("CC:98:8B:00:11:44"),
                                        .paired = true, .connected = false});
        service = std::make_shared<core::DeviceService>(simulated.transport, simulated.discovery);
        if (connect) service->connect(transport::DeviceAddress(simulated.address), simulated.name);
        controller = std::make_unique<DeviceCenterController>(nullptr, service, historyDir.path());
        model = std::make_unique<PeripheralModel>(*controller, source);
    }

    void settle() {
        QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(controller->pairedDevices().size() >= 3, 5000);
    }
    QStringList names() const {
        QStringList out;
        for (const auto& row : model->rows()) out << row.peripheral.name;
        return out;
    }
    QVariant at(int row, PeripheralModel::Role role) const { return model->data(model->index(row), role); }
    int rowOf(const QString& name) const { return int(names().indexOf(name)); }
};

} // namespace

class PeripheralModelTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void kindFromNameCoversTheUsualSuspects() {
        QCOMPARE(peripheralKindFromName("MX Master 3S"), PeripheralKind::Mouse);
        QCOMPARE(peripheralKindFromName("Keychron K3"), PeripheralKind::Keyboard);
        QCOMPARE(peripheralKindFromName("DualSense Wireless Controller"), PeripheralKind::Gamepad);
        QCOMPARE(peripheralKindFromName("Xbox Wireless Controller"), PeripheralKind::Gamepad);
        QCOMPARE(peripheralKindFromName("WF-1000XM5"), PeripheralKind::Earbuds);
        QCOMPARE(peripheralKindFromName("LinkBuds S"), PeripheralKind::Earbuds);
        QCOMPARE(peripheralKindFromName("WH-1000XM5"), PeripheralKind::Headphones);
        QCOMPARE(peripheralKindFromName("Galaxy Watch"), PeripheralKind::Other);
        QCOMPARE(peripheralKindName(PeripheralKind::Gamepad), QString("gamepad"));
    }

    void addressesNormaliseToColonSeparatedUpperCase() {
        QCOMPARE(normalizePeripheralAddress("cc:98:8b:00:11:22"), QString("CC:98:8B:00:11:22"));
        QCOMPARE(normalizePeripheralAddress("cc988b001122"), QString("CC:98:8B:00:11:22"));
        QCOMPARE(normalizePeripheralAddress("CC-98-8B-00-11-22"), QString("CC:98:8B:00:11:22"));
        QCOMPARE(normalizePeripheralAddress("garbage"), QString("GARBAGE"));
    }

    void fakeSourceOnlyAnnouncesRealChanges() {
        FakePeripheralSource source;
        QSignalSpy changed(&source, &IPeripheralSource::changed);
        source.setPeripherals(FakePeripheralSource::simulatedSet());
        QCOMPARE(changed.count(), 1);
        source.setPeripherals(FakePeripheralSource::simulatedSet());
        QCOMPARE(changed.count(), 1);
        source.update("f4:73:35:aa:10:01", [](Peripheral& p) { p.battery = 49; });
        QCOMPARE(changed.count(), 2);
        QCOMPARE(source.peripherals().first().battery, 49);
        source.update("00:00:00:00:00:00", [](Peripheral& p) { p.battery = 0; });
        QCOMPARE(changed.count(), 2);
        source.refreshLater(10);
        QTRY_COMPARE(source.refreshCount, 1);
    }

    void nullSourceIsEmptyAndUnavailable() {
        NullPeripheralSource source;
        QVERIFY(!source.isAvailable());
        QVERIFY(source.peripherals().isEmpty());
        source.refresh();
        QVERIFY(source.peripherals().isEmpty());
    }

    void sonyRowsWinTheMergeAndSortFirst() {
        Rig rig;
        rig.settle();
        QAbstractItemModelTester tester(rig.model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        // Windows lists the same headset with a lower-case address, an HFP
        // battery reading that lags the protocol's, and its own idea of the
        // name; none of that may leak into the Sony row.
        auto system = FakePeripheralSource::simulatedSet();
        system.append({.address = QString::fromStdString(rig.simulated.address).toLower(), .name = "WH-1000XM5 Hands-Free AG",
                       .kind = PeripheralKind::Headphones, .connected = true, .battery = 60});
        rig.source.setPeripherals(system);

        QCOMPARE(rig.names(), QStringList({"WH-1000XM5", "Keychron K3", "MX Master 3S", "LinkBuds S", "WH-1000XM4",
                                           "DualSense Wireless Controller"}));
        const int sony = rig.rowOf("WH-1000XM5");
        QVERIFY(rig.at(sony, PeripheralModel::SonyRole).toBool());
        QVERIFY(rig.at(sony, PeripheralModel::ActiveRole).toBool());
        QVERIFY(rig.at(sony, PeripheralModel::ConnectedRole).toBool());
        QCOMPARE(rig.at(sony, PeripheralModel::BatteryRole).toInt(), 87);
        QCOMPARE(rig.at(sony, PeripheralModel::KindRole).toString(), QString("headphones"));
        QCOMPARE(rig.at(sony, PeripheralModel::NoiseModeRole).toString(), QString("cancelling"));
        QCOMPARE(rig.at(sony, PeripheralModel::AddressRole).toString(), QString::fromStdString(rig.simulated.address).toUpper());
        // The simulator has no codec to report; a real set fills this in.
        QCOMPARE(rig.at(sony, PeripheralModel::CodecRole).toString(),
                 rig.controller->codec() == "Unknown" ? QString() : rig.controller->codec());
        QCOMPARE(rig.model->indexOf(QString::fromStdString(rig.simulated.address).toLower()), sony);

        const int mouse = rig.rowOf("MX Master 3S");
        QVERIFY(!rig.at(mouse, PeripheralModel::SonyRole).toBool());
        QCOMPARE(rig.at(mouse, PeripheralModel::BatteryRole).toInt(), 50);
        QCOMPARE(rig.at(mouse, PeripheralModel::KindRole).toString(), QString("mouse"));
        QVERIFY(rig.at(mouse, PeripheralModel::CodecRole).toString().isEmpty());

        // Paired but idle Sony sets keep their Sony identity and their kind.
        const int buds = rig.rowOf("LinkBuds S");
        QVERIFY(rig.at(buds, PeripheralModel::SonyRole).toBool());
        QVERIFY(!rig.at(buds, PeripheralModel::ConnectedRole).toBool());
        QCOMPARE(rig.at(buds, PeripheralModel::KindRole).toString(), QString("earbuds"));
        QCOMPARE(rig.at(buds, PeripheralModel::BatteryRole).toInt(), -1);

        QCOMPARE(rig.model->rowCount(), 6);
        QCOMPARE(rig.model->connectedCount(), 3);
        const auto map = rig.model->get(sony);
        QCOMPARE(map.value("name").toString(), QString("WH-1000XM5"));
        QCOMPARE(map.value("battery").toInt(), 87);
    }

    void systemFillsWhatTheControllerLacks() {
        Rig rig;
        rig.settle();
        // The OS says the idle XM4 is actually connected (to this PC, over
        // A2DP) at 42%: the Sony row picks both up because it had neither.
        rig.source.setPeripherals({{.address = "CC:98:8B:00:11:33", .name = "WH-1000XM4", .kind = PeripheralKind::Headphones,
                                    .connected = true, .battery = 42}});
        const int xm4 = rig.rowOf("WH-1000XM4");
        QVERIFY(rig.at(xm4, PeripheralModel::SonyRole).toBool());
        QVERIFY(!rig.at(xm4, PeripheralModel::ActiveRole).toBool());
        QVERIFY(rig.at(xm4, PeripheralModel::ConnectedRole).toBool());
        QCOMPARE(rig.at(xm4, PeripheralModel::BatteryRole).toInt(), 42);
        // Connected now, so it moved up next to the active set.
        QCOMPARE(rig.names().first(), QString("WH-1000XM5"));
        QCOMPARE(rig.names().at(1), QString("WH-1000XM4"));
    }

    void earbudsCarryPerSideLevels() {
        Rig rig("WF-1000XM5");
        rig.settle();
        const int buds = rig.rowOf("WF-1000XM5");
        QVERIFY(buds >= 0);
        QCOMPARE(rig.at(buds, PeripheralModel::KindRole).toString(), QString("earbuds"));
        QVERIFY(rig.at(buds, PeripheralModel::HasDualBatteryRole).toBool());
        QCOMPARE(rig.at(buds, PeripheralModel::BatteryLeftRole).toInt(), 81);
        QCOMPARE(rig.at(buds, PeripheralModel::BatteryRightRole).toInt(), 79);
        QCOMPARE(rig.at(buds, PeripheralModel::BatteryCaseRole).toInt(), 64);
        QCOMPARE(rig.at(buds, PeripheralModel::BatteryRole).toInt(), 79);
    }

    void updatesAreIncrementalNotResets() {
        Rig rig;
        rig.settle();
        QAbstractItemModelTester tester(rig.model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy resets(rig.model.get(), &QAbstractItemModel::modelReset);
        QSignalSpy inserted(rig.model.get(), &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(rig.model.get(), &QAbstractItemModel::rowsRemoved);
        QSignalSpy moved(rig.model.get(), &QAbstractItemModel::rowsMoved);
        QSignalSpy changed(rig.model.get(), &QAbstractItemModel::dataChanged);
        QSignalSpy count(rig.model.get(), &PeripheralModel::countChanged);

        rig.source.setPeripherals(FakePeripheralSource::simulatedSet());
        QCOMPARE(inserted.count(), 3);
        QCOMPARE(count.count(), 1);
        QCOMPARE(rig.model->rowCount(), 6);

        // A battery tick on the mouse touches one row and nothing else.
        changed.clear();
        rig.source.update("F4:73:35:AA:10:01", [](Peripheral& p) { p.battery = 49; });
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().at(0).toModelIndex().row(), rig.rowOf("MX Master 3S"));
        QCOMPARE(rig.at(rig.rowOf("MX Master 3S"), PeripheralModel::BatteryRole).toInt(), 49);
        QCOMPARE(inserted.count(), 3);

        // The controller ticks: same data, so the model stays quiet.
        changed.clear();
        emit rig.controller->stateChanged();
        QCOMPARE(changed.count(), 0);

        // The pad connects: it moves up into the connected block rather than
        // being removed and re-added.
        const int padBefore = rig.rowOf("DualSense Wireless Controller");
        rig.source.update("F4:73:35:AA:10:03", [](Peripheral& p) { p.connected = true; });
        QCOMPARE(moved.count(), 1);
        QCOMPARE(removed.count(), 0);
        const int padAfter = rig.rowOf("DualSense Wireless Controller");
        QVERIFY(padAfter < padBefore);
        QCOMPARE(rig.model->connectedCount(), 4);

        // Unpairing the keyboard removes exactly its row.
        auto rest = rig.source.peripherals();
        rest.removeIf([](const Peripheral& p) { return p.kind == PeripheralKind::Keyboard; });
        rig.source.setPeripherals(rest);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(rig.model->rowCount(), 5);
        QCOMPARE(rig.rowOf("Keychron K3"), -1);

        // A live battery change on the headset lands as a dataChanged too.
        changed.clear();
        rig.simulated.transport->setBattery(70, false);
        QTRY_COMPARE_WITH_TIMEOUT(rig.at(rig.rowOf("WH-1000XM5"), PeripheralModel::BatteryRole).toInt(), 70, 5000);
        QVERIFY(changed.count() >= 1);
        QCOMPARE(resets.count(), 0);
    }

    void sonyOnlyKeepsSonyLookingSystemRows() {
        Rig rig;
        rig.settle();
        QAbstractItemModelTester tester(rig.model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        auto system = FakePeripheralSource::simulatedSet();
        // A Sony set the controller has never seen: Sony by address prefix
        // (58:18:62 is a Sony OUI), plainly not by name.
        system.append({.address = "58:18:62:00:00:01", .name = "Living room", .kind = PeripheralKind::Headphones,
                       .connected = false, .battery = -1});
        rig.source.setPeripherals(system);
        QCOMPARE(rig.model->rowCount(), 7);
        QVERIFY(rig.at(rig.rowOf("Living room"), PeripheralModel::SonyRole).toBool());
        QSignalSpy resets(rig.model.get(), &QAbstractItemModel::modelReset);
        rig.model->setIncludeSystem(false);
        QCOMPARE(rig.names(), QStringList({"WH-1000XM5", "LinkBuds S", "Living room", "WH-1000XM4"}));
        QCOMPARE(resets.count(), 0);
        rig.model->setIncludeSystem(true);
        QCOMPARE(rig.model->rowCount(), 7);
    }
    void disconnectingKeepsTheSonyRowButDropsItsLiveState() {
        Rig rig;
        rig.settle();
        QAbstractItemModelTester tester(rig.model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        rig.source.setPeripherals(FakePeripheralSource::simulatedSet());
        QCOMPARE(rig.names().first(), QString("WH-1000XM5"));
        rig.service->activeDevice()->powerOff();
        QTRY_VERIFY_WITH_TIMEOUT(!rig.controller->isConnected(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.rowOf("WH-1000XM5") >= 0 && !rig.at(rig.rowOf("WH-1000XM5"), PeripheralModel::ConnectedRole).toBool(), 5000);
        const int sony = rig.rowOf("WH-1000XM5");
        QVERIFY(rig.at(sony, PeripheralModel::SonyRole).toBool());
        QVERIFY(!rig.at(sony, PeripheralModel::ActiveRole).toBool());
        QCOMPARE(rig.at(sony, PeripheralModel::BatteryRole).toInt(), -1);
        QCOMPARE(rig.at(sony, PeripheralModel::CodecRole).toString(), QString());
        // The connected peripherals now lead; the Sony sets head the idle block.
        QCOMPARE(rig.names().mid(0, 2), QStringList({"Keychron K3", "MX Master 3S"}));
        QCOMPARE(rig.names().at(2), QString("LinkBuds S"));
    }
};

QTEST_GUILESS_MAIN(PeripheralModelTests)
#include "PeripheralModelTests.moc"
