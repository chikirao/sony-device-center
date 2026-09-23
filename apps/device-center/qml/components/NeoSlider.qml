import QtQuick
import ".."
import QtQuick.Controls

// Horizontal slider: a thin sunk track, ink fill, paper knob.
Slider {
    required property var appWindow
    id: sl
    property real confirmedValue: 0
    // On the dark sidebar surfaces (the advanced panel).
    property bool inverse: false
    // On an inked surface (the mode button in use): paper and ink swap.
    property bool inked: false
    readonly property color ink: inked ? Theme.accentText : inverse ? Theme.sidebarAccent : Theme.accent
    readonly property color paper: inked ? Theme.accent : inverse ? Theme.sidebarSurfaceHi : Theme.surfaceHi
    value: confirmedValue
    // The knob shows what the user asked for until the device has gone quiet:
    // never under the finger, never while a command is in flight, and not for
    // a moment after release, because the snapshot from an intermediate value
    // used to snap a just-released knob back to where it passed through.
    function sync() { if (!pressed && !settle.running && !controller.busy) value = confirmedValue }
    onConfirmedValueChanged: sync()
    onMoved: settle.restart()
    onPressedChanged: {
        settle.restart()
        if (!pressed && Math.round(value) !== Math.round(confirmedValue)) moved()
    }
    Connections { target: controller; function onStateChanged() { if (!controller.busy) sl.sync() } }
    Timer { id: settle; interval: 600; onTriggered: sl.sync() }
    implicitHeight: 28
    hoverEnabled: true

    background: Rectangle {
        x: sl.leftPadding
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: sl.availableWidth
        height: 6
        radius: 3
        color: sl.inked ? Qt.rgba(sl.ink.r, sl.ink.g, sl.ink.b, 0.2) : sl.inverse ? Theme.sidebarSurfaceSunk : Theme.surfaceSunk
        Rectangle {
            width: sl.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: sl.ink
        }
    }

    handle: Rectangle {
        x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: 22
        height: 22
        radius: 11
        color: sl.paper
        border.width: 1
        border.color: sl.inked ? sl.ink : sl.inverse ? Theme.sidebarLineHi : Theme.lineHi
        scale: sl.pressed ? 1.12 : (sl.hovered ? 1.06 : 1.0)
        Behavior on scale { NumberAnimation { duration: Theme.duration(140) } }
        Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: sl.ink }
    }
}
