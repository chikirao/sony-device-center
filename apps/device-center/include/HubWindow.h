#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QRect>

class QQmlEngine;

namespace sony::devicecenter {

// Owns the Device Hub (qml/Hub.qml): creates it in the main window's engine
// so it shares `controller`, `peripherals` and Theme, places it next to the
// tray icon (or the cursor, when the icon's rectangle is unknown) inside
// the screen's available area, and turns tray clicks into open / close.
class HubWindow : public QObject {
    Q_OBJECT
public:
    // mainWindow is Main.qml's root; the hub's components take their icons
    // and translations from it.
    HubWindow(QQmlEngine& engine, QObject* mainWindow, QObject* parent = nullptr);
    ~HubWindow() override;

    [[nodiscard]] bool isReady() const { return _window != nullptr; }
    [[nodiscard]] QQuickWindow* window() const { return _window; }
    [[nodiscard]] bool isVisible() const;
    // Where the hub would be put for that anchor, in global coordinates.
    [[nodiscard]] QPoint placement(const QRect& anchor) const;
    // The placement rule itself: beside the anchor on the side with room,
    // clamped into the screen's available area. Pure, so tests can run it
    // without a screen or a tray.
    [[nodiscard]] static QPoint placeNear(const QRect& anchor, const QSize& size, const QRect& available);

public slots:
    // anchor: the tray icon's geometry in global coordinates, or an empty
    // rect to use the cursor position.
    void open(const QRect& anchor = {});
    void close();
    // The tray icon was clicked: open, unless the hub just closed because
    // that very click took the focus away (then the click was a "close").
    void toggle(const QRect& anchor = {});

signals:
    // A row or the footer wants the main window on that page (-1: as is).
    void mainWindowRequested(int page);
    void visibleChanged(bool visible);

private:
    QPointer<QQuickWindow> _window;
    QElapsedTimer _sinceHidden;
    bool _hiddenOnce{false};
};

} // namespace sony::devicecenter
