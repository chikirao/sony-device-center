#pragma once

#include <QObject>
#include <QString>

#include <vector>

namespace sony::devicecenter {

// Tells the app when the OS reports a Bluetooth link coming up or going
// down, so a reconnect can happen the moment the headphones are switched on
// instead of at the next backed-off retry. Windows only for now: it listens
// for HCI connection events on every radio through RegisterDeviceNotification
// on a message-only window that Qt's event loop pumps, and re-enumerates the
// radios when one arrives or leaves (Bluetooth toggled in Settings, dongle
// plugged in). Elsewhere it is inert and isAvailable() is false.
class BluetoothWatcher : public QObject {
    Q_OBJECT
public:
    explicit BluetoothWatcher(QObject* parent = nullptr);
    ~BluetoothWatcher() override;

    // True when at least one radio is being watched.
    [[nodiscard]] bool isAvailable() const { return !_registrations.empty(); }
    // 48-bit address as the rest of the app writes it: "CC:98:8B:00:11:22".
    [[nodiscard]] static QString formatAddress(quint64 address);
    // Drops the radio registrations and enumerates the radios again. Called
    // on radio arrival/removal; public for the window procedure and tests.
    void rescanRadios();

signals:
    // Any device, not just Sony ones; the receiver decides what to do.
    void connectionChanged(const QString& address, bool connected);
    void availabilityChanged();

private:
    struct Registration {
        void* radio{nullptr};
        void* notification{nullptr};
    };
    void _start();
    void _unwatchRadios();
    void _stop();

    void* _window{nullptr};
    void* _radioNotification{nullptr};
    std::vector<Registration> _registrations;
};

} // namespace sony::devicecenter
