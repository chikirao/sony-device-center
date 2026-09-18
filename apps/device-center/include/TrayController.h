#pragma once

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>

#include "PeripheralSource.h"

class QAction;
class QMenu;
class QSystemTrayIcon;
class QWindow;

namespace sony::devicecenter {

class DeviceCenterController;
class HubSettings;
class PeripheralModel;

// System tray presence: a battery-ring icon, a quick-control menu and
// show/hide of the main window. Mirrors DeviceCenterController state; it never
// talks to the device itself. With HubSettings in "perDevice" mode it also
// keeps one extra icon per chosen device from the PeripheralModel: the same
// ring and number, a small class badge, the device's name and charge as the
// tooltip, and the hub on click.
class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(DeviceCenterController& controller, QObject* parent = nullptr);
    ~TrayController() override;

    // The QML ApplicationWindow, once the engine has created it.
    void setWindow(QWindow* window);
    [[nodiscard]] bool isAvailable() const;

    // The hub's preferences and device list; without them the tray behaves
    // as it always has (one icon, click opens the hub).
    void setHubSettings(HubSettings* settings);
    void setPeripherals(PeripheralModel* peripherals);
    // Extra icons currently shown for individual devices.
    [[nodiscard]] int deviceIconCount() const { return int(_deviceIcons.size()); }

    // Rendered tray glyph: a ring showing charge level with the percentage
    // inside. Grey when disconnected. Exposed for tests and previews.
    static QIcon renderIcon(int level, bool charging, bool connected, int size = 64);
    // The same ring and number with a device-class badge in the corner, for
    // the per-device icons.
    static QIcon renderDeviceIcon(int level, bool connected, PeripheralKind kind, int size = 64);

public slots:
    void toggleWindow();
    void showWindow();
    // Desktop notification through the tray (a toast on Windows). No-op
    // without a tray.
    void showMessage(const QString& title, const QString& body);

signals:
    void quitRequested();
    // Left click: the Device Hub, anchored to the icon (an empty rect when
    // the shell will not say where the icon is). Double click goes to the
    // main window and asks the hub to step aside first.
    void hubToggleRequested(const QRect& iconGeometry);
    void hubDismissRequested();

private:
    struct DeviceIcon {
        QSystemTrayIcon* icon{nullptr};
        int level{-2};
        bool connected{false};
        PeripheralKind kind{};
        QString name;
    };
    void _buildMenu();
    void _update();
    void _syncDeviceIcons();

    DeviceCenterController& _controller;
    QSystemTrayIcon* _tray{nullptr};
    QMenu* _menu{nullptr};
    QAction* _status{nullptr};
    QAction* _noiseCancelling{nullptr};
    QAction* _ambient{nullptr};
    QAction* _off{nullptr};
    QAction* _speakToChat{nullptr};
    QAction* _powerOff{nullptr};
    QAction* _show{nullptr};
    QAction* _quit{nullptr};
    QPointer<QWindow> _window;
    QPointer<HubSettings> _settings;
    QPointer<PeripheralModel> _peripherals;
    // address -> its own tray icon, perDevice mode only.
    QHash<QString, DeviceIcon> _deviceIcons;
    // Last values the icon was drawn for; redraw only when they change.
    int _drawnLevel{-2};
    bool _drawnCharging{false};
    bool _drawnConnected{false};
};

} // namespace sony::devicecenter
