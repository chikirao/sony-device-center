import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Vertical EQ band. Fills outward from the zero line, because that's what it means.
ColumnLayout {
    required property var appWindow
    id: band
    property string label: ""
    property real value: 0
    signal moved(real v)

    spacing: 10

    Text {
        textFormat: Text.PlainText
        Layout.alignment: Qt.AlignHCenter
        text: (band.value > 0 ? "+" : "") + Math.round(band.value)
        color: Math.round(band.value) === 0 ? appWindow.txtFaint : appWindow.accentSoft
        font.pixelSize: 12
        font.weight: Font.DemiBold
        Behavior on color { ColorAnimation { duration: appWindow.tFast } }
    }

    Slider {
        id: vs
        Layout.alignment: Qt.AlignHCenter
        Layout.fillHeight: true
        orientation: Qt.Vertical
        from: -10
        to: 10
        stepSize: 1
        value: band.value
        Connections {
            target: controller
            function onStateChanged() { vs.value = Qt.binding(function() { return band.value }) }
        }
        implicitWidth: 34
        hoverEnabled: true
        onMoved: band.moved(value)

        background: Rectangle {
            x: vs.leftPadding + vs.availableWidth / 2 - width / 2
            y: vs.topPadding
            width: 6
            height: vs.availableHeight
            radius: 3
            color: appWindow.surfaceSunk
            border.width: 1
            border.color: appWindow.line

            // The zero line. Small detail, big legibility win.
            Rectangle {
                width: 14
                height: 1
                x: -4
                y: parent.height / 2
                color: appWindow.lineHi
            }

            Rectangle {
                readonly property real p: 1 - vs.visualPosition
                width: parent.width
                y: parent.height * (1 - Math.max(0.5, p))
                height: parent.height * Math.abs(p - 0.5)
                radius: 3
                gradient: Gradient {
                    GradientStop { position: 0.0; color: appWindow.accentSoft }
                    GradientStop { position: 1.0; color: appWindow.accent }
                }
            }
        }

        handle: Rectangle {
            x: vs.leftPadding + vs.availableWidth / 2 - width / 2
            y: vs.topPadding + vs.visualPosition * (vs.availableHeight - height)
            width: 20
            height: 20
            radius: 10
            color: "white"
            border.width: 2
            border.color: appWindow.accent
            scale: vs.pressed ? 1.22 : (vs.hovered ? 1.1 : 1.0)
            Behavior on scale {
                NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 2.5 }
            }
        }
    }

    Text {
        textFormat: Text.PlainText
        Layout.alignment: Qt.AlignHCenter
        text: band.label
        color: appWindow.txtFaint
        font.pixelSize: 10
        font.letterSpacing: 0.6
    }
}
