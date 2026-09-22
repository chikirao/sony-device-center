import QtQuick
import ".."
import QtQuick.Controls

// A round, outlined glyph button with a themed tooltip. `onSidebar` switches
// to the always-dark rail's palette.
Item {
    id: button
    required property var appWindow
    property string glyphPath: ""
    property string toolTip: ""
    property bool onSidebar: false
    property real size: 36
    signal clicked()

    implicitWidth: size
    implicitHeight: size
    opacity: enabled ? 1 : 0.4
    Accessible.role: Accessible.Button
    Accessible.name: toolTip
    Accessible.onPressAction: if (enabled) clicked()

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: !hover.hovered ? "transparent"
             : button.onSidebar ? Theme.sidebarSurface : Theme.surfaceHi
        border.width: 1
        border.color: hover.hovered ? (button.onSidebar ? Theme.sidebarLineHi : Theme.lineHi)
                                    : (button.onSidebar ? Theme.sidebarLine : Theme.line)
        Behavior on color { ColorAnimation { duration: Theme.tFast } }
    }
    Glyph {
        appWindow: button.appWindow
        anchors.centerIn: parent
        path: button.glyphPath
        size: Math.round(button.size * 0.46)
        weight: 1.7
        color: button.onSidebar ? (hover.hovered ? Theme.sidebarTxt : Theme.sidebarTxtDim)
                                : (hover.hovered ? Theme.txt : Theme.txtDim)
    }

    HoverHandler { id: hover; enabled: button.enabled; cursorShape: Qt.PointingHandCursor }
    TapHandler { enabled: button.enabled; onTapped: button.clicked() }

    ToolTip {
        visible: hover.hovered && button.toolTip !== ""
        delay: 500
        text: button.toolTip
        padding: 8
        contentItem: Text {
            textFormat: Text.PlainText
            text: button.toolTip
            color: Theme.txt
            font.pixelSize: 12
        }
        background: Rectangle {
            radius: Theme.controlRadius
            color: Theme.surfaceHi
            border.width: 1
            border.color: Theme.lineHi
        }
    }
}
