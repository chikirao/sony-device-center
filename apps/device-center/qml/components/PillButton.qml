import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// The pill button. Press physics live here so every button feels identical.
Button {
    required property var appWindow
    id: pill
    property color tint: Theme.accent
    property bool active: false
    property string glyphPath: ""
    property bool compact: false

    implicitHeight: compact ? 40 : 48
    // Padding is declared, not inherited, so implicitWidth can be derived
    // from it. Otherwise the control sizes contentItem to availableWidth,
    // the row is wider than it needs, and the slack lands on the right.
    topPadding: 0
    bottomPadding: 0
    leftPadding: compact ? 16 : 20
    rightPadding: leftPadding
    implicitWidth: pillRow.implicitWidth + leftPadding + rightPadding
    hoverEnabled: true
    scale: pressed ? 0.955 : (hovered ? 1.035 : 1.0)

    Behavior on scale {
        NumberAnimation { duration: Theme.duration(220); easing.type: Easing.OutBack; easing.overshoot: 2.2 }
    }

    background: Rectangle {
        radius: height / 2
        color: pill.active ? Qt.rgba(pill.tint.r, pill.tint.g, pill.tint.b, 0.16)
             : pill.hovered ? Theme.surfaceHi : Theme.surface
        border.width: 1
        border.color: pill.active ? Qt.rgba(pill.tint.r, pill.tint.g, pill.tint.b, 0.8)
                    : pill.hovered ? Theme.lineHi : Theme.line

        Behavior on color { ColorAnimation { duration: Theme.tBase } }
        Behavior on border.color { ColorAnimation { duration: Theme.tBase } }

        // Halo ring — the difference between "on" and "on, and you felt it".
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: height / 2
            color: "transparent"
            border.width: 1
            border.color: pill.tint
            opacity: pill.active ? 0.4 : 0
            scale: pill.active ? 1.0 : 0.94
            Behavior on opacity { NumberAnimation { duration: Theme.tSlow } }
            Behavior on scale { NumberAnimation { duration: Theme.tSlow; easing.type: Easing.OutBack } }
        }
    }

    contentItem: RowLayout {
        id: pillRow
        // The glyph box is 24 units wide but most paths don't fill it, so
        // the optical gap is always a few px wider than this number.
        spacing: pill.compact ? 7 : 8
        Glyph { appWindow: pill.appWindow;
            visible: pill.glyphPath !== ""
            path: pill.glyphPath
            size: pill.compact ? 17 : 20
            weight: 1.9
            color: pill.active ? pill.tint : Theme.txtDim
        }
        Text {
            textFormat: Text.PlainText
            text: pill.text
            color: pill.active ? Theme.txt : Theme.txtDim
            font.pixelSize: pill.compact ? 12 : 13
            font.weight: pill.active ? Font.DemiBold : Font.Medium
            Behavior on color { ColorAnimation { duration: Theme.tFast } }
        }
    }
}
