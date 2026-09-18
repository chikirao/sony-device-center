#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

namespace sony::devicecenter {

// What a Bluetooth peripheral looks like to the OS: enough to draw one row
// in the Device Hub. Sony headsets come through here too (the OS sees them
// like any other headset) and get merged with the live controller state by
// PeripheralModel.
enum class PeripheralKind { Headphones, Earbuds, Mouse, Keyboard, Gamepad, Other };

struct Peripheral {
    // "CC:98:8B:00:11:22", upper case; the merge key across sources.
    QString address;
    QString name;
    PeripheralKind kind{PeripheralKind::Other};
    bool connected{false};
    // 0..100, -1 when the OS reports nothing. Earbuds may add per-side and
    // case levels, also -1 when unknown.
    int battery{-1};
    int batteryLeft{-1};
    int batteryRight{-1};
    int batteryCase{-1};

    bool operator==(const Peripheral&) const = default;
};

// Lower-case identifier used by QML for glyph lookup and by tests.
QString peripheralKindName(PeripheralKind kind);
// Best guess from a product name alone ("MX Master 3S" -> mouse); Other when
// nothing in the name gives it away.
PeripheralKind peripheralKindFromName(const QString& name);
// "cc:98:8b:00:11:22", "CC988B001122" and "CC:98:8B:00:11:22" all become the
// last form; anything unparseable is returned upper-cased as is.
QString normalizePeripheralAddress(const QString& address);

// Where the list of system Bluetooth devices comes from. Implementations
// poll the OS on their own schedule and emit changed() whenever the list they
// hand out differs from the last one.
class IPeripheralSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    // Ask for a fresh enumeration now (the hub opened).
    virtual void refresh() = 0;
    // The same, a moment later: after a link event the OS takes a beat to
    // flip IsConnected and read the first battery report, and a burst of
    // events collapses into one refresh.
    void refreshLater(int delayMs = 1500);
    [[nodiscard]] virtual QList<Peripheral> peripherals() const = 0;
    // False when this platform has no implementation; the UI then hides the
    // system-device parts instead of showing an empty list forever.
    [[nodiscard]] virtual bool isAvailable() const = 0;
    // How often the OS is re-enumerated on its own; 0 disables polling.
    virtual void setPollInterval(int) {}

signals:
    void changed();

private:
    QTimer* _later{nullptr};
};

// Platforms without an implementation (Linux, macOS): nothing, never.
class NullPeripheralSource : public IPeripheralSource {
    Q_OBJECT
public:
    using IPeripheralSource::IPeripheralSource;
    void refresh() override {}
    [[nodiscard]] QList<Peripheral> peripherals() const override { return {}; }
    [[nodiscard]] bool isAvailable() const override { return false; }
};

// Hand-fed list for tests and for --simulated, where a mouse, a keyboard and
// a controller keep the hub company.
class FakePeripheralSource : public IPeripheralSource {
    Q_OBJECT
public:
    using IPeripheralSource::IPeripheralSource;
    void refresh() override { ++refreshCount; emit changed(); }
    [[nodiscard]] QList<Peripheral> peripherals() const override { return _peripherals; }
    [[nodiscard]] bool isAvailable() const override { return true; }

    void setPeripherals(QList<Peripheral> peripherals);
    // Changes one device in place (by address); a no-op for unknown ones.
    void update(const QString& address, const std::function<void(Peripheral&)>& change);
    // The three the simulator ships with.
    static QList<Peripheral> simulatedSet();

    int refreshCount{0};

private:
    QList<Peripheral> _peripherals;
};

// The OS-backed source for this platform, or a NullPeripheralSource where
// there is none.
IPeripheralSource* createPlatformPeripheralSource(QObject* parent = nullptr);

} // namespace sony::devicecenter
