import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Horizontal slider with a gradient fill and a handle that reacts.
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
    implicitHeight: 26
    hoverEnabled: true

    background: Rectangle {
        x: sl.leftPadding
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: sl.availableWidth
        height: 6
        radius: 3
        color: appWindow.surfaceSunk
        border.width: 1
        border.color: appWindow.line

        Rectangle {
            width: sl.visualPosition * parent.width
            height: parent.height
            radius: 3
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: appWindow.accent }
                GradientStop { position: 1.0; color: appWindow.accentSoft }
            }
        }
    }

    handle: Rectangle {
        x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
        y: sl.topPadding + sl.availableHeight / 2 - height / 2
        width: 18
        height: 18
        radius: 9
        color: "white"
        border.width: 2
        border.color: appWindow.accent
        scale: sl.pressed ? 1.25 : (sl.hovered ? 1.12 : 1.0)
        Behavior on scale {
            NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 2.5 }
        }
    }
}
