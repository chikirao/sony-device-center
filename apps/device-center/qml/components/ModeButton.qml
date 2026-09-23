import QtQuick
import ".."
import QtQuick.Layouts

// One noise mode to pick. Two shapes: a row (glyph, title and detail, a
// radio at the end) for a column of them, and a tile (glyph over title)
// for three abreast. The one in use is inked, with a paper dot for a mark.
Rectangle {
    id: button
    required property var appWindow
    property string glyphPath: ""
    property string title: ""
    property string detail: ""
    property bool current: false
    property bool tile: false
    signal clicked()

    readonly property color fg: current ? Theme.accentText : Theme.txt
    readonly property color fgDim: current ? Qt.rgba(Theme.accentText.r, Theme.accentText.g, Theme.accentText.b, 0.62) : Theme.txtDim

    implicitHeight: tile ? 96 : 84
    radius: Theme.cardRadius + 2
    color: current ? Theme.accent : hover.hovered && enabled ? Theme.surfaceHi : Theme.surface
    border.width: 1
    border.color: current ? Theme.accent : hover.hovered && enabled ? Theme.lineHi : Theme.line
    opacity: enabled ? 1 : 0.5
    Behavior on color { ColorAnimation { duration: Theme.tBase } }
    Behavior on border.color { ColorAnimation { duration: Theme.tBase } }

    Accessible.role: Accessible.RadioButton
    Accessible.name: title
    Accessible.checked: current
    Accessible.onPressAction: if (enabled) clicked()

    HoverHandler { id: hover; enabled: button.enabled; cursorShape: Qt.PointingHandCursor }
    TapHandler { enabled: button.enabled; onTapped: button.clicked() }

    // Row
    RowLayout {
        visible: !button.tile
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        spacing: 16
        Glyph { appWindow: button.appWindow; path: button.glyphPath; size: 22; weight: 1.6; color: button.fg }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: button.title
                color: button.fg
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: text !== ""
                text: button.detail
                color: button.fgDim
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
        // Radio: a hollow ring, filled with a paper dot when in use.
        Item {
            implicitWidth: 16
            implicitHeight: 16
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.width: 1.2
                border.color: Theme.lineHi
                opacity: button.current ? 0 : 1
                Behavior on opacity { NumberAnimation { duration: Theme.tBase } }
            }
            Rectangle {
                anchors.centerIn: parent
                width: 10; height: 10; radius: 5
                color: Theme.accentText
                scale: button.current ? 1 : 0
                Behavior on scale { NumberAnimation { duration: Theme.duration(260); easing.type: Easing.OutBack } }
            }
        }
    }

    // Tile
    ColumnLayout {
        visible: button.tile
        anchors.centerIn: parent
        width: parent.width - 16
        spacing: 3
        Glyph {
            appWindow: button.appWindow
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 7
            path: button.glyphPath; size: 24; weight: 1.6; color: button.fg
        }
        // Two lines rather than an ellipsis when three tiles share a
        // narrow window ("Noise / Cancelling").
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: button.title
            color: button.fg
            font.pixelSize: 13
            font.weight: Font.DemiBold
            // Two lines' room: "Окружающий / звук" wraps, a single long
            // word ("Шумоподавление") shrinks a size or two rather than
            // breaking mid-word.
            Layout.preferredHeight: 32
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            fontSizeMode: Text.Fit
            minimumPixelSize: 10
            lineHeight: 0.95
        }
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            visible: text !== ""
            text: button.detail
            color: button.fgDim
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
    Rectangle {
        visible: button.tile
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        width: 7; height: 7; radius: 3.5
        color: Theme.accentText
        scale: button.current ? 1 : 0
        Behavior on scale { NumberAnimation { duration: Theme.duration(260); easing.type: Easing.OutBack } }
    }
}
