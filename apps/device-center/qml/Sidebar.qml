import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

// Always-dark rail: wordmark, navigation, and the device at the foot.
Rectangle {
    id: root
    required property var appWindow
    Layout.fillHeight: true
    Layout.preferredWidth: 212
    color: Theme.sidebarBg

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.sidebarLine
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        anchors.topMargin: 26
        spacing: 0

        // Wordmark
        RowLayout {
            Layout.leftMargin: 6
            spacing: 12
            BrandMark { appWindow: root.appWindow; size: 30; color: Theme.sidebarTxt }
            Text {
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
                    height: 48
                    radius: 10
                    color: current ? Theme.sidebarSurface : navHover.hovered ? Theme.sidebarSurfaceSunk : "transparent"
                    border.width: 1
                    border.color: current ? Theme.sidebarLineHi : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.tFast } }

                    HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: appWindow.navIndex = navItem.modelData.idx }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 14
                        Glyph { appWindow: root.appWindow;
                            path: navItem.modelData.glyph
                            size: 20
                            weight: navItem.modelData.idx === 3 ? 2.6 : 1.6
                            color: navItem.current ? Theme.sidebarTxt : navHover.hovered ? Theme.sidebarTxtDim : Theme.sidebarTxtDim
                        }
                        Text {
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
        RowLayout {
            Layout.topMargin: 18
            // The dot lines up with the navigation glyphs above.
            Layout.leftMargin: 20
            Layout.rightMargin: 4
            Layout.bottomMargin: 8
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
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
        }
    }
}
