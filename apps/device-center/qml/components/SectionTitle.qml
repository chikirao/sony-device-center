import QtQuick
import ".."
import QtQuick.Layouts

// Page heading: optional eyebrow, a heavy title and one calm line under it.
ColumnLayout {
    required property var appWindow
    id: root
    property string eyebrow: ""
    property string title: ""
    property string subtitle: ""
    spacing: 4
    Eyebrow { appWindow: root.appWindow; visible: root.eyebrow !== ""; text: root.eyebrow; Layout.bottomMargin: 4 }
    Text {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        text: root.title
        color: Theme.txt
        font.pixelSize: root.appWindow.compact ? 26 : 34
        font.weight: Font.Bold
        font.letterSpacing: root.appWindow.compact ? -0.6 : -1
        wrapMode: Text.Wrap
    }
    Text {
        textFormat: Text.PlainText
        visible: root.subtitle !== ""
        text: root.subtitle
        color: Theme.txtDim
        font.pixelSize: root.appWindow.compact ? 13 : 15
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
