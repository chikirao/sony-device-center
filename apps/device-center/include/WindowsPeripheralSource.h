#pragma once

#include "PeripheralSource.h"

#include <QThread>
#include <QTimer>

namespace sony::devicecenter {

class PeripheralScanner;

// Paired Bluetooth devices as Windows sees them. Enumeration goes through
// Windows.Devices.Enumeration (classic and LE association endpoints, with
// System.Devices.Aep.IsConnected); the charge comes from the
// DEVPKEY_Bluetooth_Battery property Windows keeps on the Hands-Free device
// node of classic headsets (fed over HFP, so coarser than what the Sony
// protocol reports), and from the GATT Battery Service (0x180F) for
// connected LE devices that expose one. All of it runs on a worker thread,
// where the blocking WinRT calls are harmless; the GUI thread only ever
// sees the finished list.
// Compiled to a stub elsewhere, but createPlatformPeripheralSource() never
// picks it there.
class WindowsPeripheralSource : public IPeripheralSource {
    Q_OBJECT
public:
    explicit WindowsPeripheralSource(QObject* parent = nullptr);
    ~WindowsPeripheralSource() override;

    void refresh() override;
    [[nodiscard]] QList<Peripheral> peripherals() const override { return _peripherals; }
    [[nodiscard]] bool isAvailable() const override;
    void setPollInterval(int seconds) override;

    // How many scans have completed; tests and logging.
    [[nodiscard]] int scanCount() const { return _scanCount; }

signals:
    // Internal: wakes the scanner on its own thread.
    void scanRequested();

private:
    void _apply(const QList<Peripheral>& peripherals);

    QThread _thread;
    PeripheralScanner* _scanner{nullptr};
    QTimer _poll;
    QList<Peripheral> _peripherals;
    bool _scanning{false};
    bool _again{false};
    int _scanCount{0};
};

} // namespace sony::devicecenter
