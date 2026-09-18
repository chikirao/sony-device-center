#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <memory>

namespace sony::devicecenter {

// The Device Hub's own preferences, persisted under the `hub/` group of the
// app's QSettings: whether the hub lists every Bluetooth device or Sony sets
// only, how often the OS is re-read, what a click on the tray icon opens,
// and whether the tray shows one icon (following the connected Sony set, as
// it always has) or one per chosen device.
class HubSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool showSystemDevices READ showSystemDevices WRITE setShowSystemDevices NOTIFY changed)
    Q_PROPERTY(int pollIntervalSeconds READ pollIntervalSeconds WRITE setPollIntervalSeconds NOTIFY changed)
    Q_PROPERTY(QString trayClickAction READ trayClickAction WRITE setTrayClickAction NOTIFY changed)
    Q_PROPERTY(QString trayMode READ trayMode WRITE setTrayMode NOTIFY changed)
    Q_PROPERTY(QStringList trayDevices READ trayDevices NOTIFY changed)
public:
    // iniPath: a settings file to use instead of the app's registry/ini
    // store; tests point it at a temporary file.
    explicit HubSettings(const QString& iniPath = {}, QObject* parent = nullptr);

    // Hub lists the OS's devices too (true) or only Sony sets (false).
    [[nodiscard]] bool showSystemDevices() const { return _showSystemDevices; }
    void setShowSystemDevices(bool on);
    // 15..600; the peripheral source's polling period.
    [[nodiscard]] int pollIntervalSeconds() const { return _pollIntervalSeconds; }
    void setPollIntervalSeconds(int seconds);
    // "hub" (default) or "window": what a left click on the tray icon opens.
    [[nodiscard]] QString trayClickAction() const { return _trayClickAction; }
    void setTrayClickAction(const QString& action);
    // "single" (default): the one icon that follows the connected Sony set.
    // "perDevice": that icon plus one per address in trayDevices().
    [[nodiscard]] QString trayMode() const { return _trayMode; }
    void setTrayMode(const QString& mode);
    // Addresses (normalised, upper case) that get an icon in perDevice mode.
    [[nodiscard]] QStringList trayDevices() const { return _trayDevices; }
    Q_INVOKABLE bool isInTray(const QString& address) const;
    Q_INVOKABLE void setInTray(const QString& address, bool on);

signals:
    void changed();

private:
    std::unique_ptr<QSettings> _store;
    bool _showSystemDevices{true};
    int _pollIntervalSeconds{30};
    QString _trayClickAction{"hub"};
    QString _trayMode{"single"};
    QStringList _trayDevices;
};

} // namespace sony::devicecenter
