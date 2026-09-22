import QtQuick
import ".."
import QtQuick.Layouts

// The strip every page shares: which device, and its three vital signs.
RowLayout {
    required property var appWindow
    id: header
    spacing: 8

    readonly property string modeLabel: !controller.connected || controller.noiseControlMode === "unknown" ? "—"
        : controller.noiseControlMode === "cancelling" ? appWindow.tr("mode_anc")
        : controller.noiseControlMode === "ambient" ? appWindow.tr("mode_ambient") : appWindow.tr("mode_off")
    readonly property string batteryLabel: !controller.connected ? "—"
        : controller.hasDualBattery ? Math.min(controller.batteryLeft < 0 ? 100 : controller.batteryLeft,
                                               controller.batteryRight < 0 ? 100 : controller.batteryRight) + "%"
        : controller.batteryLevel >= 0 ? controller.batteryLevel + "%" : "—"

    ColumnLayout {
        id: nameColumn
        spacing: 6
        Layout.fillWidth: true
        // An explicit floor: otherwise the dot name's natural width becomes
        // the layout minimum and the chips push past the window edge.
        Layout.minimumWidth: 200
        // The name and the chips are different kinds of thing; keep them
        // visibly apart rather than one chip-gap away.
        Layout.rightMargin: 16
        DotText {
            objectName: "deviceNameDots"
            text: controller.deviceName
            dot: 6
            maxWidth: nameColumn.width
            color: controller.connected ? Theme.txt : Theme.txtFaint
            Behavior on color { ColorAnimation { duration: Theme.tBase } }
        }
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: controller.hasDualBattery ? appWindow.tr("wireless_earbuds") : appWindow.tr("wireless_headphones")
            color: Theme.txtDim
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }

    Repeater {
        model: [
            { k: appWindow.tr("codec"),      v: controller.connected && controller.codec.length ? controller.codec : "—", g: appWindow.icons.waveform },
            { k: appWindow.tr("battery"),    v: header.batteryLabel, g: controller.isCharging ? appWindow.icons.bolt : appWindow.icons.batteryUp },
            { k: appWindow.tr("sound_mode"), v: header.modeLabel, g: appWindow.icons.ambient }
        ]
        // Compact: a glyph, a small label and a short dot value. The
        // pages below carry the detail; these are a glance.
        delegate: Card {
            id: chip
            appWindow: header.appWindow
            required property var modelData
            required property int index
            implicitWidth: chipRow.implicitWidth + 32
            implicitHeight: 58
            radius: Theme.controlRadius
            Layout.alignment: Qt.AlignVCenter
            RowLayout {
                id: chipRow
                anchors.centerIn: parent
                spacing: 10
                Glyph { appWindow: header.appWindow; path: chip.modelData.g; size: 16; color: Theme.txtDim; weight: 1.6 }
                ColumnLayout {
                    spacing: 6
                    Eyebrow { appWindow: header.appWindow; text: chip.modelData.k }
                    DotText { text: chip.modelData.v; dot: 2.4; maxWidth: 84; color: Theme.txt; delay: chip.index * 120 }
                }
            }
        }
    }

    // Top right, where the Sony app keeps it. Same action as the hub's
    // power control: no confirmation, like there.
    IconButton {
        objectName: "headerPowerOff"
        appWindow: header.appWindow
        Layout.alignment: Qt.AlignVCenter
        Layout.leftMargin: 4
        glyphPath: appWindow.icons.power
        toolTip: appWindow.tr("power_off")
        enabled: controller.connected
        onClicked: controller.powerOff()
    }
}
