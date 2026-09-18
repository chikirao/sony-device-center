#include "HubSettings.h"
#include "PeripheralSource.h"

#include <algorithm>

namespace sony::devicecenter {

HubSettings::HubSettings(const QString& iniPath, QObject* parent) : QObject(parent) {
    _store = iniPath.isEmpty() ? std::make_unique<QSettings>("SonyBridge", "SonyDeviceCenter")
                               : std::make_unique<QSettings>(iniPath, QSettings::IniFormat);
    _store->beginGroup("hub");
    _showSystemDevices = _store->value("showSystemDevices", true).toBool();
    _pollIntervalSeconds = std::clamp(_store->value("pollIntervalSeconds", 30).toInt(), 15, 600);
    _trayClickAction = _store->value("trayClickAction", "hub").toString();
    if (_trayClickAction != "hub" && _trayClickAction != "window") _trayClickAction = "hub";
    _trayMode = _store->value("trayMode", "single").toString();
    if (_trayMode != "single" && _trayMode != "perDevice") _trayMode = "single";
    for (const auto& address : _store->value("trayDevices").toStringList()) {
        const auto key = normalizePeripheralAddress(address);
        if (!key.isEmpty() && !_trayDevices.contains(key)) _trayDevices.append(key);
    }
}

void HubSettings::setShowSystemDevices(bool on) {
    if (_showSystemDevices == on) return;
    _showSystemDevices = on;
    _store->setValue("showSystemDevices", on);
    emit changed();
}

void HubSettings::setPollIntervalSeconds(int seconds) {
    seconds = std::clamp(seconds, 15, 600);
    if (_pollIntervalSeconds == seconds) return;
    _pollIntervalSeconds = seconds;
    _store->setValue("pollIntervalSeconds", seconds);
    emit changed();
}

void HubSettings::setTrayClickAction(const QString& action) {
    if (action != "hub" && action != "window") return;
    if (_trayClickAction == action) return;
    _trayClickAction = action;
    _store->setValue("trayClickAction", action);
    emit changed();
}

void HubSettings::setTrayMode(const QString& mode) {
    if (mode != "single" && mode != "perDevice") return;
    if (_trayMode == mode) return;
    _trayMode = mode;
    _store->setValue("trayMode", mode);
    emit changed();
}

bool HubSettings::isInTray(const QString& address) const {
    return _trayDevices.contains(normalizePeripheralAddress(address));
}

void HubSettings::setInTray(const QString& address, bool on) {
    const auto key = normalizePeripheralAddress(address);
    if (key.isEmpty() || isInTray(key) == on) return;
    if (on) _trayDevices.append(key);
    else _trayDevices.removeAll(key);
    _store->setValue("trayDevices", _trayDevices);
    emit changed();
}

} // namespace sony::devicecenter
