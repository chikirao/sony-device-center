import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Small uppercase label. Letter-spacing is what makes it read as deliberate.
Text {
    id: root
    required property var appWindow
    textFormat: Text.PlainText
    color: appWindow.txtFaint
    font.pixelSize: 10
    font.bold: true
    font.letterSpacing: 1.4
    font.capitalization: Font.AllUppercase
}
