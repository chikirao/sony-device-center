#pragma once

#include <QSettings>

namespace sony::devicecenter {

// The app's own preferences: the registry on Windows, a plist on macOS, an
// ini file on Linux. Every store goes through this class, not
// QSettings("SonyBridge", "SonyDeviceCenter"): that constructor always picks
// the native format, while this one follows QSettings::defaultFormat(), so
// the tests can send all of it to a scratch directory with
// setDefaultFormat(IniFormat) and setPath(). No Q_OBJECT: it adds no signals.
class AppSettings : public QSettings {
public:
    AppSettings() : QSettings(defaultFormat(), UserScope, QStringLiteral("SonyBridge"), QStringLiteral("SonyDeviceCenter")) {}
};

} // namespace sony::devicecenter
