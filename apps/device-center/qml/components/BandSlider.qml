import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts

// Vertical EQ band: ink fills from the floor up to the knob.
ColumnLayout {
    required property var appWindow
    id: band
    property string label: ""
    property real value: 0
    signal moved(real v)

    // A layout's maximum width is its widest child's; without this the
    // column can never grow past the slider and the bands bunch up left.
    Layout.maximumWidth: Number.POSITIVE_INFINITY
    spacing: 10

    Text {
        textFormat: Text.PlainText
        Layout.alignment: Qt.AlignHCenter
        text: (band.value > 0 ? "+" : "") + Math.round(band.value)
        color: Math.round(band.value) === 0 ? Theme.txtFaint : Theme.txt
        font.pixelSize: 12
        font.weight: Font.DemiBold
        Behavior on color { ColorAnimation { duration: Theme.tFast } }
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
            function onStateChanged() { if (!vs.pressed) vs.value = Qt.binding(function() { return band.value }) }
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
            color: Theme.surfaceSunk
            Rectangle {
                width: parent.width
                y: vs.visualPosition * parent.height
                height: parent.height - y
                radius: 3
                color: Theme.accent
            }
        }

        handle: Rectangle {
            x: vs.leftPadding + vs.availableWidth / 2 - width / 2
            y: vs.topPadding + vs.visualPosition * (vs.availableHeight - height)
            width: 22
            height: 22
            radius: 11
            color: Theme.surfaceHi
            border.width: 1
            border.color: Theme.lineHi
            scale: vs.pressed ? 1.12 : (vs.hovered ? 1.06 : 1.0)
            Behavior on scale { NumberAnimation { duration: Theme.duration(140) } }
            Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: Theme.accent }
        }
    }

    Text {
        textFormat: Text.PlainText
        Layout.alignment: Qt.AlignHCenter
        text: band.label
        color: Theme.txtDim
        font.pixelSize: 11
    }
}
