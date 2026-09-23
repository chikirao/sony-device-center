import QtQuick
import ".."
import QtQuick.Layouts

// The strip every page shares: which device, how full it is, and the way
// into the advanced panel.
RowLayout {
    required property var appWindow
    id: header
    spacing: 8

    readonly property int batteryPercent: !controller.connected ? -1
        : controller.hasDualBattery ? Math.min(controller.batteryLeft < 0 ? 100 : controller.batteryLeft,
                                               controller.batteryRight < 0 ? 100 : controller.batteryRight)
        : controller.batteryLevel
    readonly property string batteryLabel: batteryPercent >= 0 ? batteryPercent + "%" : "—"
    readonly property bool batteryLow: !controller.isCharging && batteryPercent >= 0 && batteryPercent <= 20

    ColumnLayout {
        id: nameColumn
        spacing: 6
        Layout.fillWidth: true
        // An explicit floor: otherwise the dot name's natural width becomes
        // the layout minimum and pushes the battery past the window edge.
        Layout.minimumWidth: 120
        Layout.rightMargin: 16
        DotText {
            objectName: "deviceNameDots"
            text: controller.deviceName
            dot: appWindow.compact ? 2.6 : 3.6
            maxWidth: nameColumn.width
            color: controller.connected ? Theme.txt : Theme.txtFaint
            Behavior on color { ColorAnimation { duration: Theme.tBase } }
        }
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: controller.hasDualBattery ? appWindow.tr("wireless_earbuds") : appWindow.tr("wireless_headphones")
            color: Theme.txtDim
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    // Battery: a glyph and the level on the dot grid, nothing around it.
    // Earbuds show the emptier bud; the Battery page has each one.
    RowLayout {
        objectName: "headerBattery"
        Layout.alignment: Qt.AlignVCenter
        Layout.rightMargin: 12
        spacing: 7
        Glyph {
            appWindow: header.appWindow
            path: controller.isCharging ? appWindow.icons.bolt : appWindow.icons.batteryUp
            size: 16
            weight: 1.6
            color: header.batteryLow ? Theme.danger : Theme.txt
        }
        DotText {
            objectName: "headerBatteryDots"
            text: header.batteryLabel
            dot: 2.6
            maxWidth: 110
            color: header.batteryLow ? Theme.danger : Theme.txt
        }
        HoverHandler { id: batteryHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: appWindow.navIndex = 5 }
    }

    // Everything the Overview leaves out lives behind this.
    IconButton {
        objectName: "advancedButton"
        appWindow: header.appWindow
        Layout.alignment: Qt.AlignVCenter
        size: 40
        glyphPath: appWindow.icons.settings
        toolTip: appWindow.tr("advanced")
        onClicked: appWindow.advancedOpen = true
    }
}
