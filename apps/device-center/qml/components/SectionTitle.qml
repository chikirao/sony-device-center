import QtQuick
import ".."

// Page heading: optional eyebrow, a heavy title and one calm line under it.
// A plain Column, not a layout: its texts take their width straight from
// the page, so a wrapped line settles in one pass. Nested in the page's
// layout, a ColumnLayout here needed a third pass with Linux fonts.
Column {
    required property var appWindow
    id: root
    property string eyebrow: ""
    property string title: ""
    property string subtitle: ""
    spacing: 4
    Item {
        visible: root.eyebrow !== ""
        width: eyebrowText.implicitWidth
        height: eyebrowText.implicitHeight + 4
        Eyebrow { id: eyebrowText; appWindow: root.appWindow; text: root.eyebrow }
    }
    Text {
        textFormat: Text.PlainText
        width: root.width
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
        width: root.width
        text: root.subtitle
        color: Theme.txtDim
        font.pixelSize: root.appWindow.compact ? 13 : 15
        wrapMode: Text.Wrap
    }
}
