import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

// Always-dark rail: wordmark, navigation, and the device at the foot. In a
// narrow window it folds to its icons, with the labels as tooltips.
Rectangle {
    id: root
    required property var appWindow
    readonly property bool folded: appWindow.compact
    Layout.fillHeight: true
    Layout.preferredWidth: folded ? 68 : 212
    color: Theme.sidebarBg

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.sidebarLine
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.folded ? 12 : 18
        anchors.topMargin: 26
        spacing: 0

        // Wordmark
        RowLayout {
            Layout.leftMargin: root.folded ? 7 : 6
            spacing: 12
            BrandMark { appWindow: root.appWindow; size: 30; color: Theme.sidebarTxt }
            Text {
                visible: !root.folded
                textFormat: Text.PlainText
                text: "Device"
                color: Theme.sidebarTxt
                font.pixelSize: 22
                font.weight: Font.Bold
                font.letterSpacing: -0.5
            }
        }

        // Navigation
        ColumnLayout {
            Layout.topMargin: 30
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: [
                    { idx: 0, key: "nav_overview",        glyph: appWindow.icons.home },
                    { idx: 1, key: "nav_noise_control",   glyph: appWindow.icons.waveform },
                    { idx: 2, key: "nav_equalizer",       glyph: appWindow.icons.sliders },
                    { idx: 3, key: "nav_audio_features",  glyph: appWindow.icons.gridDots },
                    { idx: 4, key: "nav_device_switcher", glyph: appWindow.icons.swap },
                    { idx: 5, key: "nav_battery",         glyph: appWindow.icons.batteryUp },
                    { idx: 6, key: "nav_settings",        glyph: appWindow.icons.settings }
                ]
                delegate: Rectangle {
                    id: navItem
                    required property var modelData
                    readonly property bool current: appWindow.navIndex === modelData.idx
                    Layout.fillWidth: true
                    height: root.folded ? 44 : 48
                    radius: 10
                    Accessible.role: Accessible.PageTab
                    Accessible.name: appWindow.tr(modelData.key)
                    color: current ? Theme.sidebarSurface : navHover.hovered ? Theme.sidebarSurfaceSunk : "transparent"
                    border.width: 1
                    border.color: current ? Theme.sidebarLineHi : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.tFast } }

                    HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: appWindow.navIndex = navItem.modelData.idx }

                    ToolTip {
                        visible: root.folded && navHover.hovered
                        delay: 400
                        x: navItem.width + 10
                        y: (navItem.height - height) / 2
                        text: appWindow.tr(navItem.modelData.key)
                        padding: 8
                        contentItem: Text { textFormat: Text.PlainText; text: appWindow.tr(navItem.modelData.key); color: Theme.txt; font.pixelSize: 12 }
                        background: Rectangle { radius: Theme.controlRadius; color: Theme.surfaceHi; border.width: 1; border.color: Theme.lineHi }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: root.folded ? 12 : 14
                        anchors.rightMargin: root.folded ? 12 : 14
                        spacing: 14
                        Glyph { appWindow: root.appWindow;
                            path: navItem.modelData.glyph
                            size: 20
                            weight: navItem.modelData.idx === 3 ? 2.6 : 1.6
                            color: navItem.current ? Theme.sidebarTxt : navHover.hovered ? Theme.sidebarTxtDim : Theme.sidebarTxtDim
                        }
                        Text {
                            visible: !root.folded
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr(navItem.modelData.key)
                            elide: Text.ElideRight
                            color: navItem.current ? Theme.sidebarTxt : navHover.hovered ? Theme.sidebarTxt : Theme.sidebarTxtDim
                            font.pixelSize: 13
                            font.weight: navItem.current ? Font.DemiBold : Font.Medium
                            Behavior on color { ColorAnimation { duration: Theme.tFast } }
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.sidebarLine }

        // Connection state at the foot: a live dot and a word. The device
        // name is the header's; the sidebar only says whether it is there.
        // Folded, the word goes and the dot sits above the button.
        GridLayout {
            Layout.topMargin: 18
            // The dot lines up with the navigation glyphs above.
            Layout.leftMargin: root.folded ? 0 : 20
            Layout.rightMargin: root.folded ? 0 : 4
            Layout.bottomMargin: 8
            Layout.alignment: root.folded ? Qt.AlignHCenter : Qt.AlignLeft
            columns: root.folded ? 1 : 2
            rowSpacing: 14
            columnSpacing: 10
            RowLayout {
                Layout.fillWidth: !root.folded
                Layout.alignment: Qt.AlignHCenter
                spacing: 10
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: controller.connected ? Theme.sidebarSuccess : Theme.sidebarTxtFaint
                    SequentialAnimation on opacity {
                        running: Theme.motionEnabled && controller.connected
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.35; duration: Theme.duration(1200); easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: Theme.duration(1200); easing.type: Easing.InOutQuad }
                    }
                }
                Text {
                    objectName: "sidebarConnectionState"
                    visible: !root.folded
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    // Not connected: say why ("Connecting", "Disconnected by
                    // you"), which the footer used to spell out.
                    readonly property string stateText: appWindow.trState(controller.connectionState)
                    text: controller.connected ? (controller.isCharging ? appWindow.tr("charging") : appWindow.tr("connected"))
                                               : stateText.charAt(0).toUpperCase() + stateText.slice(1)
                    elide: Text.ElideRight
                    color: controller.connected ? Theme.sidebarTxt : Theme.sidebarTxtDim
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }
            }
            IconButton {
                objectName: "sidebarPowerOff"
                appWindow: root.appWindow
                onSidebar: true
                size: 30
                glyphPath: appWindow.icons.power
                toolTip: appWindow.tr("power_off")
                enabled: controller.connected
                onClicked: controller.powerOff()
            }
        }
    }
}
