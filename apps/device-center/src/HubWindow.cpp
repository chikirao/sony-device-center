#include "HubWindow.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

#include <algorithm>

namespace sony::devicecenter {

namespace {
// Gap between the hub and the tray icon / screen edge.
constexpr int kMargin = 8;
// A click on the tray icon steals the hub's focus, which closes it, and
// only then does the click itself arrive. Anything this soon after a
// focus-loss close is that same click and must not reopen the hub.
constexpr qint64 kReopenGuardMs = 350;
}

HubWindow::HubWindow(QQmlEngine& engine, QObject* mainWindow, QObject* parent) : QObject(parent) {
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/Hub.qml")));
    if (component.isError()) {
        for (const auto& error : component.errors()) qWarning().noquote() << "Hub:" << error.toString();
        return;
    }
    auto* context = new QQmlContext(engine.rootContext(), this);
    context->setContextProperty("mainWindow", mainWindow);
    auto* object = component.create(context);
    _window = qobject_cast<QQuickWindow*>(object);
    if (!_window) {
        for (const auto& error : component.errors()) qWarning().noquote() << "Hub:" << error.toString();
        delete object;
        return;
    }
    _window->QObject::setParent(this);
    connect(_window, SIGNAL(mainWindowRequested(int)), this, SIGNAL(mainWindowRequested(int)));
    connect(_window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) { _sinceHidden.start(); _hiddenOnce = true; }
        emit visibleChanged(visible);
    });
}

HubWindow::~HubWindow() = default;

bool HubWindow::isVisible() const { return _window && _window->isVisible(); }

QPoint HubWindow::placement(const QRect& anchorIn) const {
    const QRect anchor = anchorIn.isValid() ? anchorIn : QRect(QCursor::pos(), QSize(1, 1));
    QScreen* screen = QGuiApplication::screenAt(anchor.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    return placeNear(anchor, _window ? _window->size() : QSize(360, 240), available);
}

QPoint HubWindow::placeNear(const QRect& anchor, const QSize& size, const QRect& available) {
    // Above a bottom taskbar, below a top one, and pushed inward from a
    // vertical one; centred on the icon where the screen allows.
    int y = anchor.center().y() > available.center().y() ? anchor.top() - size.height() - kMargin
                                                         : anchor.bottom() + kMargin;
    int x = anchor.center().x() - size.width() / 2;
    if (anchor.left() <= available.left()) x = anchor.right() + kMargin;
    else if (anchor.right() >= available.right()) x = anchor.left() - size.width() - kMargin;
    x = std::clamp(x, available.left() + kMargin, std::max(available.left() + kMargin, available.right() - size.width() - kMargin));
    y = std::clamp(y, available.top() + kMargin, std::max(available.top() + kMargin, available.bottom() - size.height() - kMargin));
    return {x, y};
}

void HubWindow::open(const QRect& anchor) {
    if (!_window) return;
    _window->setPosition(placement(anchor));
    QMetaObject::invokeMethod(_window, "present");
}

void HubWindow::close() {
    if (!_window) return;
    QMetaObject::invokeMethod(_window, "dismiss");
}

void HubWindow::toggle(const QRect& anchor) {
    if (!_window) return;
    if (_window->isVisible()) { close(); return; }
    if (_hiddenOnce && _sinceHidden.elapsed() < kReopenGuardMs) return;
    open(anchor);
}

} // namespace sony::devicecenter
