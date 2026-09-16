import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Custom switch. The stock one belongs to a different app.
Switch {
    required property var appWindow
    id: sw
    property bool confirmedChecked: false
    checked: confirmedChecked
    Connections {
        target: controller
        function onStateChanged() { sw.checked = Qt.binding(function() { return sw.confirmedChecked }) }
    }
    implicitWidth: 50
    implicitHeight: 28
    hoverEnabled: true

    indicator: Rectangle {
        implicitWidth: 50
        implicitHeight: 28
        radius: height / 2
        color: sw.checked ? appWindow.accent : appWindow.surfaceSunk
        border.width: 1
        border.color: sw.checked ? appWindow.accent : (sw.hovered ? appWindow.lineHi : appWindow.line)
        Behavior on color { ColorAnimation { duration: appWindow.tBase } }
        Behavior on border.color { ColorAnimation { duration: appWindow.tBase } }

        Rectangle {
            width: 20
            height: 20
            radius: 10
            y: 4
            x: sw.checked ? parent.width - width - 4 : 4
            color: sw.checked ? "white" : appWindow.txtFaint
            Behavior on x { NumberAnimation { duration: 240; easing.type: Easing.OutBack; easing.overshoot: 1.4 } }
            Behavior on color { ColorAnimation { duration: appWindow.tBase } }
        }
    }
    contentItem: Item {}
}
