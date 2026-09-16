import QtQuick
import ".."
import QtQuick.Controls

// Horizontal slider: a thin sunk track, ink fill, paper knob.
Slider {
    required property var appWindow
    id: sl
    property real confirmedValue: 0
    value: confirmedValue
    // Re-attach the binding once the device confirms, but never under the
    // user's finger: a poll arriving mid-drag would yank the handle back.
    Connections {
        target: controller
        function onStateChanged() { if (!sl.pressed) sl.value = Qt.binding(function() { return sl.confirmedValue }) }
    }
    implicitHeight: 28
    hoverEnabled: true

    background: Rectangle {
        x: sl.leftPadding
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: sl.availableWidth
        height: 6
        radius: 3
        color: Theme.surfaceSunk
        Rectangle {
            width: sl.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: 22
        height: 22
        radius: 11
        color: Theme.surfaceHi
        border.width: 1
        border.color: Theme.lineHi
        scale: sl.pressed ? 1.12 : (sl.hovered ? 1.06 : 1.0)
        Behavior on scale { NumberAnimation { duration: Theme.duration(140) } }
        Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: Theme.accent }
    }
}
