import QtQuick
import ".."
import QtQuick.Controls

// Picker: a quiet field whose chevron turns while the list is open.
ComboBox {
    required property var appWindow
    id: combo
    implicitHeight: 40

    background: Rectangle {
        radius: Theme.controlRadius
        color: combo.hovered ? Theme.surfaceHi : Theme.surface
        border.width: 1
        border.color: combo.hovered ? Theme.lineHi : Theme.line
        Behavior on color { ColorAnimation { duration: Theme.tFast } }
    }
    contentItem: Text {
        textFormat: Text.PlainText
        leftPadding: 14
        rightPadding: 30
        text: combo.displayText
        color: Theme.txt
        font.pixelSize: 12
        font.weight: Font.Medium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Glyph { appWindow: combo.appWindow;
        x: combo.width - width - 12
        y: combo.height / 2 - height / 2
        size: 14
        color: Theme.txtDim
        path: combo.appWindow.icons.chevron
        rotation: combo.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: Theme.tBase } }
    }
}
