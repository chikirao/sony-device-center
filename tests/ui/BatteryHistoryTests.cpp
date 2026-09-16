#include <QtTest>
#include "BatteryHistory.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <memory>
using namespace sony::devicecenter;

namespace {
constexpr qint64 kMinute = 60 * 1000;
constexpr qint64 kHour = 60 * kMinute;
constexpr qint64 kDay = 24 * kHour;
// A fixed "now" keeps the numbers below readable; nothing in the class
// consults the wall clock.
constexpr qint64 kT0 = 1'700'000'000'000;
const QString kAddress = "CC:98:8B:00:11:22";
}

class BatteryHistoryTests : public QObject {
    Q_OBJECT
    // Fresh storage for every test: the log persists by design, so a shared
    // directory would leak one test's samples into the next.
    std::unique_ptr<QTemporaryDir> _dir;
    QString dir() const { return _dir->path(); }

    // Discharging session: connected at 100%, then one point per percent.
    static void discharge(BatteryHistory& h, qint64 from, int startLevel, int steps, qint64 stepMs) {
        h.observe(from, true, startLevel, false);
        for (int i = 1; i <= steps; ++i) h.observe(from + i * stepMs, true, startLevel - i, false);
    }

private slots:
    void init() { _dir = std::make_unique<QTemporaryDir>(); }
    void cleanup() { _dir.reset(); }

    void recordsOnlyChanges() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        QSignalSpy changed(&h, &BatteryHistory::changed);
        for (int i = 0; i < 10; ++i) h.observe(kT0 + i * 500, true, 87, false);
        QCOMPARE(h.samples().size(), 1);
        QCOMPARE(h.samples()[0].event, BatteryHistory::Event::Connected);
        QCOMPARE(h.samples()[0].level, 87);
        h.observe(kT0 + 10 * kMinute, true, 86, false);
        QCOMPARE(h.samples().size(), 2);
        QCOMPARE(h.samples()[1].event, BatteryHistory::Event::Level);
        // A charger going on at the same level is a change too.
        h.observe(kT0 + 11 * kMinute, true, 86, true);
        QCOMPARE(h.samples().size(), 3);
        QVERIFY(h.samples()[2].charging);
        QCOMPARE(changed.count(), 3);
    }

    void marksConnectAndDisconnect() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        // Nothing to log while the device is away or has no reading yet.
        h.observe(kT0, false, -1, false);
        h.observe(kT0 + 1, true, -1, false);
        QVERIFY(h.samples().isEmpty());
        h.observe(kT0 + 2, true, 80, false);
        h.observe(kT0 + kMinute, false, -1, false);
        h.observe(kT0 + 2 * kMinute, false, -1, false);
        h.observe(kT0 + kHour, true, 75, false);
        QCOMPARE(h.samples().size(), 3);
        QCOMPARE(h.samples()[1].event, BatteryHistory::Event::Disconnected);
        QCOMPARE(h.samples()[1].level, 80);   // carries the last known level for the chart
        QCOMPARE(h.samples()[2].event, BatteryHistory::Event::Connected);
        QCOMPARE(h.samples()[2].level, 75);
    }

    void noDeviceMeansNothingIsRecorded() {
        BatteryHistory h(dir());
        h.observe(kT0, true, 80, false);
        QVERIFY(h.samples().isEmpty());
        QVERIFY(h.filePath().isEmpty());
    }

    void estimateNeedsADropAndSomeTime() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        QVERIFY(!h.estimate(kT0).valid);
        h.observe(kT0, true, 100, false);
        QVERIFY2(!h.estimate(kT0 + kHour).valid, "one sample is not a rate");
        // The connect sample sits at an unknown fraction, so the first level
        // change is only the anchor, not yet a drop.
        h.observe(kT0 + 10 * kMinute, true, 99, false);
        QVERIFY(!h.estimate(kT0 + 10 * kMinute).valid);
        h.observe(kT0 + 20 * kMinute, true, 98, false);
        QVERIFY2(!h.estimate(kT0 + 20 * kMinute).valid, "1% drop is below the minimum");
        h.observe(kT0 + 30 * kMinute, true, 97, false);
        const auto e = h.estimate(kT0 + 30 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, kT0);
        // 2% over 20 minutes = 6%/h; 97% lasts 970 minutes.
        QCOMPARE(e.percentPerHour, 6.0);
        QCOMPARE(e.remainingMs / kMinute, 970);
        // Time keeps running between samples.
        QCOMPARE(h.estimate(kT0 + 35 * kMinute).remainingMs / kMinute, 965);
    }

    void tooShortASessionIsNotTrusted() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        // 3% in three minutes: the drop is there, the span is not.
        discharge(h, kT0, 100, 4, kMinute);
        QVERIFY(!h.estimate(kT0 + 4 * kMinute).valid);
        h.observe(kT0 + 10 * kMinute, true, 95, false);
        QVERIFY(h.estimate(kT0 + 10 * kMinute).valid);
    }

    void estimateRestartsWhenChargingStops() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 10, 10 * kMinute);   // 6%/h, ends at 50% after 100 min
        QVERIFY(h.estimate(kT0 + 100 * kMinute).valid);
        // On the charger: no estimate at all.
        h.observe(kT0 + 2 * kHour, true, 50, true);
        h.observe(kT0 + 2 * kHour + 5 * kMinute, true, 55, true);
        QVERIFY(!h.estimate(kT0 + 2 * kHour + 5 * kMinute).valid);
        // Off the charger at 100%: the old rate must not be reused.
        const qint64 off = kT0 + 4 * kHour;
        h.observe(off, true, 100, false);
        QVERIFY(!h.estimate(off + kHour).valid);
        h.observe(off + 30 * kMinute, true, 99, false);
        h.observe(off + 90 * kMinute, true, 97, false);
        const auto e = h.estimate(off + 90 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, off);
        QCOMPARE(e.percentPerHour, 2.0);
        QCOMPARE(e.remainingMs / kMinute, 97 * 30);
    }

    void estimateIsUnknownWhileDisconnected() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 10, 10 * kMinute);
        h.observe(kT0 + 2 * kHour, false, -1, false);
        QVERIFY(!h.estimate(kT0 + 2 * kHour).valid);
    }

    void shortReconnectKeepsTheSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);   // 6%/h
        // A one-minute Bluetooth hiccup.
        h.observe(kT0 + 61 * kMinute, false, -1, false);
        h.observe(kT0 + 62 * kMinute, true, 54, false);
        h.observe(kT0 + 70 * kMinute, true, 53, false);
        const auto e = h.estimate(kT0 + 70 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, kT0);
        QCOMPARE(e.percentPerHour, 6.0);
    }

    void restartAtTheSameLevelKeepsTheSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);   // 6%/h, last point 54 at +60 min
        // The app was relaunched: no disconnect marker, a fresh connect at
        // the level the log already ends with.
        {
            BatteryHistory again(dir());
            again.setDevice(kAddress);
            again.observe(kT0 + 61 * kMinute, true, 54, false);
            QCOMPARE(again.samples().last().event, BatteryHistory::Event::Connected);
            again.observe(kT0 + 70 * kMinute, true, 53, false);
            const auto e = again.estimate(kT0 + 70 * kMinute);
            QVERIFY(e.valid);
            QCOMPARE(e.sessionStartMs, kT0);
            QCOMPARE(e.percentPerHour, 6.0);
        }
        // Relaunched at a different level: the headset was used elsewhere.
        BatteryHistory later(dir());
        later.setDevice(kAddress);
        later.observe(kT0 + 71 * kMinute, true, 40, false);
        QVERIFY(!later.estimate(kT0 + 71 * kMinute).valid);
    }

    void longReconnectStartsANewSession() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 60, 6, 10 * kMinute);
        // Away for the night; the level fell while it was elsewhere.
        h.observe(kT0 + 61 * kMinute, false, -1, false);
        const qint64 back = kT0 + 10 * kHour;
        h.observe(back, true, 30, false);
        QVERIFY2(!h.estimate(back).valid, "no data from this session yet");
        h.observe(back + 10 * kMinute, true, 29, false);
        h.observe(back + 40 * kMinute, true, 27, false);
        const auto e = h.estimate(back + 40 * kMinute);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, back);
        QCOMPARE(e.percentPerHour, 4.0);
    }

    void persistsAcrossInstancesPerDevice() {
        {
            BatteryHistory h(dir());
            h.setDevice(kAddress);
            discharge(h, kT0, 90, 3, kMinute);
            h.setDevice("AA:BB:CC:DD:EE:FF");
            h.observe(kT0, true, 40, true);
            QCOMPARE(h.samples().size(), 1);
        }
        QVERIFY(QFile::exists(dir() + "/battery-history/CC-98-8B-00-11-22.json"));
        QVERIFY(QFile::exists(dir() + "/battery-history/AA-BB-CC-DD-EE-FF.json"));
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        QCOMPARE(h.samples().size(), 4);
        QCOMPARE(h.samples()[0].event, BatteryHistory::Event::Connected);
        QCOMPARE(h.samples()[3].level, 87);
        QCOMPARE(h.samples()[3].timeMs, kT0 + 3 * kMinute);
        // A fresh instance starts a new connection marker rather than
        // continuing the old line.
        h.observe(kT0 + 4 * kMinute, true, 87, false);
        QCOMPARE(h.samples().size(), 5);
        QCOMPARE(h.samples()[4].event, BatteryHistory::Event::Connected);

        h.setDevice("AA:BB:CC:DD:EE:FF");
        QCOMPARE(h.samples().size(), 1);
        QVERIFY(h.samples()[0].charging);
    }

    void fileIsCompactJson() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        h.observe(kT0, true, 87, false);
        QFile file(h.filePath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root.value("version").toInt(), 1);
        QCOMPARE(root.value("address").toString(), kAddress);
        const auto sample = root.value("samples").toArray().first().toObject();
        QCOMPARE(static_cast<qint64>(sample.value("t").toDouble()), kT0);
        QCOMPARE(sample.value("level").toInt(), 87);
        QCOMPARE(sample.value("event").toString(), QString("connected"));
    }

    void oldSamplesAreDropped() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        h.observe(kT0 - 200 * kDay, true, 50, false);
        h.observe(kT0 - 100 * kDay, true, 49, false);
        h.observe(kT0, true, 48, false);
        QCOMPARE(h.samples().size(), 2);
        QCOMPARE(h.samples()[0].timeMs, kT0 - 100 * kDay);
    }

    void samplesSinceKeepsOnePriorPointForTheChart() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        discharge(h, kT0, 90, 5, kHour);
        const auto window = h.samplesSince(kT0 + 3 * kHour + kMinute);
        QCOMPARE(window.size(), 3);   // t+3h (prior), t+4h, t+5h
        const auto first = window.first().toMap();
        QCOMPARE(first.value("t").toLongLong(), kT0 + 3 * kHour);
        QCOMPARE(first.value("level").toInt(), 87);
        QCOMPARE(first.value("event").toString(), QString("level"));
        QCOMPARE(first.value("charging").toBool(), false);
        QCOMPARE(h.samplesSince(kT0 + kDay).size(), 1);
    }

    void demoSeedGivesTheSimulatorAnEstimate() {
        BatteryHistory h(dir());
        h.setDevice(kAddress);
        h.seedDemoData(kT0);
        QVERIFY(h.samples().size() > 500);
        for (int i = 1; i < h.samples().size(); ++i) QVERIFY(h.samples()[i].timeMs >= h.samples()[i - 1].timeMs);
        const auto e = h.estimate(kT0);
        QVERIFY(e.valid);
        QCOMPARE(e.sessionStartMs, kT0 - 2 * kHour);
        QVERIFY(e.percentPerHour > 3.0 && e.percentPerHour < 5.0);
        // The simulator's first report matches the seed's last point, so it
        // continues the line instead of starting a new session.
        const auto before = h.samples().size();
        h.observe(kT0 + 1000, true, 87, false);
        QCOMPARE(h.samples().size(), before);
        h.observe(kT0 + 3000, true, 86, false);
        QCOMPARE(h.samples().last().event, BatteryHistory::Event::Level);
    }

    void sanitizesAddresses() {
        QCOMPARE(BatteryHistory::sanitizeAddress("CC:98:8B:00:11:22"), QString("CC-98-8B-00-11-22"));
        QCOMPARE(BatteryHistory::sanitizeAddress("../x\\y z"), QString("xyz"));
    }
};

QTEST_GUILESS_MAIN(BatteryHistoryTests)
#include "BatteryHistoryTests.moc"
