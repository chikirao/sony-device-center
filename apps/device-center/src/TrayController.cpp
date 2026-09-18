#include "TrayController.h"
#include "DeviceCenterController.h"
#include "HubSettings.h"
#include "PeripheralModel.h"
#include "WindowsToast.h"

#include <QAction>
#include <QActionGroup>
#include <QFont>
#include <QGuiApplication>
#include <QPolygonF>
#include <QAbstractItemModel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QWindow>

#include <algorithm>

namespace sony::devicecenter {

namespace {
// Same palette as the sidebar battery ring in Main.qml.
constexpr auto kRingTrack = "#3A3D48";
constexpr auto kRingOk = "#2DD4A7";
constexpr auto kRingLow = "#FF5A5F";
constexpr auto kRingCharging = "#7C8CFF";
constexpr auto kText = "#FFFFFF";
constexpr auto kTextDim = "#9A9EAD";
constexpr int kLowBattery = 20;
}

TrayController::TrayController(DeviceCenterController& controller, QObject* parent)
    : QObject(parent), _controller(controller) {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    _tray = new QSystemTrayIcon(this);
    _buildMenu();
    _tray->setContextMenu(_menu);
    _update();
    _tray->show();

    connect(_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        // Left click opens the hub, double click the main window (the first
        // click of a double click has already opened the hub by then, so it
        // is sent away again); the context menu is Qt's own.
        if (reason == QSystemTrayIcon::Trigger) {
            if (_settings && _settings->trayClickAction() == "window") toggleWindow();
            else emit hubToggleRequested(_tray->geometry());
        } else if (reason == QSystemTrayIcon::DoubleClick) { emit hubDismissRequested(); showWindow(); }
    });
    connect(&_controller, &DeviceCenterController::stateChanged, this, &TrayController::_update);
    connect(&_controller, &DeviceCenterController::capabilitiesChanged, this, &TrayController::_update);
    connect(&_controller, &DeviceCenterController::languageChanged, this, &TrayController::_update);
}

TrayController::~TrayController() {
    for (auto& entry : _deviceIcons) delete entry.icon;
}

void TrayController::setHubSettings(HubSettings* settings) {
    if (_settings) disconnect(_settings, nullptr, this, nullptr);
    _settings = settings;
    if (_settings) connect(_settings, &HubSettings::changed, this, &TrayController::_syncDeviceIcons);
    _syncDeviceIcons();
}

void TrayController::setPeripherals(PeripheralModel* peripherals) {
    if (_peripherals) disconnect(_peripherals, nullptr, this, nullptr);
    _peripherals = peripherals;
    if (_peripherals) {
        for (auto signal : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved})
            connect(_peripherals, signal, this, &TrayController::_syncDeviceIcons);
        connect(_peripherals, &QAbstractItemModel::dataChanged, this, &TrayController::_syncDeviceIcons);
        connect(_peripherals, &QAbstractItemModel::modelReset, this, &TrayController::_syncDeviceIcons);
        connect(_peripherals, &QAbstractItemModel::rowsMoved, this, &TrayController::_syncDeviceIcons);
    }
    _syncDeviceIcons();
}

void TrayController::_syncDeviceIcons() {
    // Which addresses deserve an icon right now: chosen in the settings,
    // present in the list, and the mode says so. Everything else goes.
    QHash<QString, const PeripheralModel::Row*> wanted;
    if (_tray && _settings && _peripherals && _settings->trayMode() == "perDevice")
        for (const auto& row : _peripherals->rows())
            if (_settings->isInTray(row.peripheral.address)) wanted.insert(row.peripheral.address, &row);

    for (auto it = _deviceIcons.begin(); it != _deviceIcons.end();) {
        if (wanted.contains(it.key())) { ++it; continue; }
        delete it->icon;
        it = _deviceIcons.erase(it);
    }
    for (auto it = wanted.begin(); it != wanted.end(); ++it) {
        const auto& p = it.value()->peripheral;
        auto& entry = _deviceIcons[it.key()];
        if (!entry.icon) {
            entry.icon = new QSystemTrayIcon;
            connect(entry.icon, &QSystemTrayIcon::activated, this, [this, icon = entry.icon](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) emit hubToggleRequested(icon->geometry());
            });
        }
        if (entry.level != p.battery || entry.connected != p.connected || entry.kind != p.kind) {
            entry.level = p.battery; entry.connected = p.connected; entry.kind = p.kind;
            entry.icon->setIcon(renderDeviceIcon(p.battery, p.connected, p.kind));
        }
        QString tip = p.name;
        if (!p.connected) tip += " — " + _controller.t("hub_not_connected");
        else if (p.battery >= 0) tip += " — " + QString::number(p.battery) + "%";
        if (entry.name != tip) { entry.name = tip; entry.icon->setToolTip(tip); }
        if (!entry.icon->isVisible()) entry.icon->show();
    }
}

QIcon TrayController::renderDeviceIcon(int level, bool connected, PeripheralKind kind, int size) {
    QPixmap pixmap = renderIcon(level, false, connected, size).pixmap(size, size);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    // Badge in the lower right corner, on a disc so it reads on any ring.
    const qreal badge = size * 0.42;
    const QRectF disc(size - badge, size - badge, badge, badge);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(kRingTrack));
    p.drawEllipse(disc);
    QPen pen(QColor(kText), std::max(1.0, size * 0.045), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF g = disc.adjusted(badge * 0.24, badge * 0.24, -badge * 0.24, -badge * 0.24);
    switch (kind) {
    case PeripheralKind::Mouse:
        p.drawRoundedRect(QRectF(g.left() + g.width() * 0.2, g.top(), g.width() * 0.6, g.height()), g.width() * 0.3, g.width() * 0.3);
        p.drawLine(QPointF(g.center().x(), g.top()), QPointF(g.center().x(), g.top() + g.height() * 0.4));
        break;
    case PeripheralKind::Keyboard:
        p.drawRoundedRect(QRectF(g.left(), g.top() + g.height() * 0.25, g.width(), g.height() * 0.5), 1.5, 1.5);
        p.drawLine(QPointF(g.left() + g.width() * 0.3, g.center().y()), QPointF(g.right() - g.width() * 0.3, g.center().y()));
        break;
    case PeripheralKind::Gamepad:
        p.drawRoundedRect(QRectF(g.left(), g.top() + g.height() * 0.25, g.width(), g.height() * 0.5), g.height() * 0.25, g.height() * 0.25);
        p.setBrush(QColor(kText));
        p.drawEllipse(QPointF(g.right() - g.width() * 0.28, g.center().y()), pen.widthF() * 0.9, pen.widthF() * 0.9);
        p.drawEllipse(QPointF(g.left() + g.width() * 0.28, g.center().y()), pen.widthF() * 0.9, pen.widthF() * 0.9);
        break;
    case PeripheralKind::Earbuds:
        p.setBrush(QColor(kText));
        p.drawEllipse(QPointF(g.left() + g.width() * 0.3, g.center().y() - g.height() * 0.1), g.width() * 0.16, g.width() * 0.16);
        p.drawEllipse(QPointF(g.right() - g.width() * 0.3, g.center().y() - g.height() * 0.1), g.width() * 0.16, g.width() * 0.16);
        break;
    case PeripheralKind::Headphones:
        p.drawArc(QRectF(g.left(), g.top(), g.width(), g.height() * 1.3), 0, 180 * 16);
        p.setBrush(QColor(kText));
        p.drawRoundedRect(QRectF(g.left(), g.center().y(), g.width() * 0.22, g.height() * 0.45), 1, 1);
        p.drawRoundedRect(QRectF(g.right() - g.width() * 0.22, g.center().y(), g.width() * 0.22, g.height() * 0.45), 1, 1);
        break;
    case PeripheralKind::Other:
        // The Bluetooth rune, simplified to its zig-zag.
        p.drawPolyline(QPolygonF({QPointF(g.left() + g.width() * 0.2, g.top() + g.height() * 0.3),
                                  QPointF(g.right() - g.width() * 0.25, g.bottom() - g.height() * 0.3),
                                  QPointF(g.center().x(), g.bottom()), QPointF(g.center().x(), g.top()),
                                  QPointF(g.right() - g.width() * 0.25, g.top() + g.height() * 0.3),
                                  QPointF(g.left() + g.width() * 0.2, g.bottom() - g.height() * 0.3)}));
        break;
    }
    return QIcon(pixmap);
}

bool TrayController::isAvailable() const { return _tray != nullptr; }

void TrayController::setWindow(QWindow* window) { _window = window; }

void TrayController::_buildMenu() {
    _menu = new QMenu();
    _status = _menu->addAction(QString());
    _status->setEnabled(false);
    _menu->addSeparator();

    // The three noise modes behave as radio buttons.
    auto* modes = new QActionGroup(_menu);
    modes->setExclusive(true);
    _noiseCancelling = _menu->addAction(QString());
    _ambient = _menu->addAction(QString());
    _off = _menu->addAction(QString());
    for (auto* action : {_noiseCancelling, _ambient, _off}) {
        action->setCheckable(true);
        modes->addAction(action);
    }
    connect(_noiseCancelling, &QAction::triggered, &_controller, [this] { _controller.setAnc(true); });
    connect(_ambient, &QAction::triggered, &_controller, [this] {
        // Re-apply the level the device last reported; the controller clamps it.
        _controller.setAmbient(_controller.ambientLevel(), _controller.focusOnVoice());
    });
    connect(_off, &QAction::triggered, &_controller, [this] { _controller.setNoiseControlOff(); });

    _speakToChat = _menu->addAction(QString());
    _speakToChat->setCheckable(true);
    connect(_speakToChat, &QAction::triggered, &_controller, [this](bool checked) { _controller.setSpeakToChat(checked); });
    _menu->addSeparator();

    _powerOff = _menu->addAction(QString());
    connect(_powerOff, &QAction::triggered, &_controller, [this] { _controller.powerOff(); });
    _menu->addSeparator();

    _show = _menu->addAction(QString());
    connect(_show, &QAction::triggered, this, &TrayController::showWindow);
    _quit = _menu->addAction(QString());
    connect(_quit, &QAction::triggered, this, &TrayController::quitRequested);
}

void TrayController::_update() {
    if (!_tray) return;
    const bool connected = _controller.isConnected();
    const int level = _controller.batteryLevel();
    const bool charging = _controller.isCharging();
    const auto mode = _controller.noiseControlMode();
    auto t = [this](const char* key) { return _controller.t(QString::fromLatin1(key)); };

    // Texts are reassigned on every update so a language switch is picked up
    // without rebuilding the menu (rebuilding would close it if it is open).
    QString status;
    if (!connected) status = t("disconnected");
    else if (level >= 0) {
        status = _controller.deviceName() + " · " + QString::number(level) + "%" + (charging ? " ⚡" : "");
        // The estimate is empty until the discharge session has enough data.
        const auto left = _controller.batteryTimeLeft();
        if (!left.isEmpty()) status += " · " + t("battery_time_left_short").arg(left);
    } else status = _controller.deviceName();
    _status->setText(status);
    _tray->setToolTip("Sony Device Center — " + status);

    _noiseCancelling->setText(t("noise_cancelling"));
    _ambient->setText(t("ambient_sound"));
    _off->setText(t("noise_control_off"));
    _speakToChat->setText("Speak-to-Chat");
    _powerOff->setText(t("power_off"));
    _show->setText(t("tray_show"));
    _quit->setText(t("tray_quit"));

    const bool busy = _controller.busy();
    _noiseCancelling->setEnabled(connected && !busy && _controller.hasAnc());
    _ambient->setEnabled(connected && !busy && _controller.hasAmbient());
    _off->setEnabled(connected && !busy);
    _noiseCancelling->setChecked(mode == "cancelling");
    _ambient->setChecked(mode == "ambient");
    _off->setChecked(mode == "off");
    _speakToChat->setVisible(_controller.hasSpeakToChat());
    _speakToChat->setEnabled(connected && !busy);
    _speakToChat->setChecked(_controller.speakToChat());
    _powerOff->setEnabled(connected && !busy);

    if (level != _drawnLevel || charging != _drawnCharging || connected != _drawnConnected) {
        _drawnLevel = level; _drawnCharging = charging; _drawnConnected = connected;
        _tray->setIcon(renderIcon(level, charging, connected));
    }
}

QIcon TrayController::renderIcon(int level, bool charging, bool connected, int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const qreal stroke = size * 0.11;
    const QRectF ring(stroke / 2, stroke / 2, size - stroke, size - stroke);
    QPen pen(QColor(kRingTrack), stroke, Qt::SolidLine, Qt::FlatCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ring);

    const bool known = connected && level >= 0;
    if (known && level > 0) {
        pen.setColor(QColor(charging ? kRingCharging : level > kLowBattery ? kRingOk : kRingLow));
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        // Qt angles are in 1/16 degree, counter-clockwise from 3 o'clock.
        p.drawArc(ring, 90 * 16, -static_cast<int>(360 * 16 * level / 100.0));
    }

    // A tray icon is ~16 px on screen, so the number is the whole message:
    // bold, and as large as the ring allows.
    QFont font;
    font.setBold(true);
    font.setPixelSize(known && level >= 100 ? static_cast<int>(size * 0.40) : static_cast<int>(size * 0.50));
    p.setFont(font);
    p.setPen(QColor(known ? kText : kTextDim));
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, known ? QString::number(level) : QStringLiteral("–"));
    return QIcon(pixmap);
}

void TrayController::showMessage(const QString& title, const QString& body) {
    if (!_tray) return;
    // Native toasts where available; the tray balloon is the portable fallback.
    if (WindowsToast::show(title, body)) return;
    _tray->showMessage(title, body, QSystemTrayIcon::Information, 6000);
}

void TrayController::toggleWindow() {
    if (!_window) return;
    if (_window->isVisible() && _window->isActive()) _window->hide();
    else showWindow();
}

void TrayController::showWindow() {
    if (!_window) return;
    _window->show();
    if (_window->windowState() & Qt::WindowMinimized) _window->showNormal();
    _window->raise();
    _window->requestActivate();
}

} // namespace sony::devicecenter
