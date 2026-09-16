import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "components"

Rectangle {
    id: root
    required property var appWindow
    Layout.fillHeight: true
    Layout.preferredWidth: 258
    color: Theme.light ? "#101112" : Qt.rgba(0.05, 0.055, 0.075, 0.72)

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.sidebarLine
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 26

        // Brand. Two lines, set to the height of the mark — the same
        // lockup the site and the brand assets use.
        RowLayout {
            spacing: 12
            BrandTile { appWindow: root.appWindow;
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
            }
            ColumnLayout {
                spacing: 2
                Text {
                    textFormat: Text.PlainText
                    text: "DEVICE"
                    color: Theme.sidebarTxtDim
                    font.family: Theme.monoFamily
                    font.pixelSize: 12
                    font.letterSpacing: 1.7
                }
                Text {
                    textFormat: Text.PlainText
                    text: "Center"
                    color: Theme.sidebarAccentSoft
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.3
                }
            }
        }

        // Device badge with a live battery ring
        Card { inverse: true; appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            active: controller.connected

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 13

                Item {
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 46

                    Canvas {
                        id: ring
                        anchors.fill: parent
                        property real level: controller.connected ? Math.max(0, controller.batteryLevel) : 0
                        Behavior on level { NumberAnimation { duration: Theme.duration(700); easing.type: Easing.OutCubic } }
                        onLevelChanged: requestPaint()

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            var cx = width / 2, cy = height / 2, r = width / 2 - 4
                            ctx.lineWidth = 3.5
                            ctx.lineCap = "round"

                            ctx.strokeStyle = "#22252F"
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, 0, Math.PI * 2)
                            ctx.stroke()

                            if (level > 0) {
                                ctx.strokeStyle = level > 20 ? "#2DD4A7" : "#FF5A5F"
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * (level / 100))
                                ctx.stroke()
                            }
                        }
                    }

                    Glyph { appWindow: root.appWindow;
                        anchors.centerIn: parent
                        visible: controller.isCharging
                        path: appWindow.icons.bolt
                        size: 16
                        color: Theme.sidebarSuccess
                        weight: 2

                        SequentialAnimation on opacity {
                            running: Theme.motionEnabled && controller.isCharging
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.35; duration: Theme.duration(900); easing.type: Easing.InOutQuad }
                            NumberAnimation { to: 1.0; duration: Theme.duration(900); easing.type: Easing.InOutQuad }
                        }
                    }

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        visible: !controller.isCharging
                        text: controller.connected && controller.batteryLevel >= 0 ? controller.batteryLevel : "—"
                        color: Theme.sidebarTxt
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: controller.deviceName
                        color: Theme.sidebarTxt
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    // Earbuds carry three batteries; the ring shows the
                    // weakest one, this line shows all of them.
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        visible: controller.connected && controller.hasDualBattery
                        text: {
                            var parts = []
                            // No "%" here: the ring next to it already says these are percentages,
                            // and the sidebar is too narrow for the long form.
                            if (controller.batteryLeft >= 0) parts.push("L " + controller.batteryLeft)
                            if (controller.batteryRight >= 0) parts.push("R " + controller.batteryRight)
                            if (controller.batteryCase >= 0) parts.push(appWindow.tr("battery_case") + " " + controller.batteryCase)
                            return parts.join(" · ")
                        }
                        color: Theme.sidebarTxtDim
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 6
                            Layout.preferredHeight: 6
                            radius: 3
                            color: controller.connected ? Theme.sidebarSuccess : Theme.sidebarDanger

                            SequentialAnimation on opacity {
                                running: Theme.motionEnabled && controller.connected
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: Theme.duration(1100); easing.type: Easing.InOutQuad }
                                NumberAnimation { to: 1.0; duration: Theme.duration(1100); easing.type: Easing.InOutQuad }
                            }
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: controller.connected ? appWindow.tr("connected") : appWindow.tr("disconnected")
                            color: controller.connected ? Theme.sidebarTxtDim : Theme.sidebarTxtFaint
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        // Navigation with a sliding indicator
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 7 * 44 + 6 * 6

            // The indicator floats; items don't each carry their own.
            Rectangle {
                width: parent.width
                height: 44
                radius: Theme.controlRadius
                y: appWindow.navIndex * 50
                color: Qt.rgba(Theme.sidebarAccent.r, Theme.sidebarAccent.g, Theme.sidebarAccent.b, 0.14)
                border.width: 1
                border.color: Qt.rgba(Theme.sidebarAccent.r, Theme.sidebarAccent.g, Theme.sidebarAccent.b, 0.45)

                Behavior on y {
                    NumberAnimation { duration: Theme.duration(320); easing.type: Easing.OutBack; easing.overshoot: 1.1 }
                }

                Rectangle {
                    width: 3
                    height: 18
                    radius: 2
                    x: -1
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.sidebarAccent
                }
            }

            Repeater {
                model: [
                    { idx: 0, key: "nav_overview",        glyph: appWindow.icons.headphones },
                    { idx: 1, key: "nav_noise_control",   glyph: appWindow.icons.shield },
                    { idx: 2, key: "nav_equalizer",       glyph: appWindow.icons.sliders },
                    { idx: 3, key: "nav_audio_features",  glyph: appWindow.icons.sparkle },
                    { idx: 4, key: "nav_device_switcher", glyph: appWindow.icons.swap },
                    { idx: 5, key: "nav_battery",         glyph: appWindow.icons.battery },
                    { idx: 6, key: "nav_settings",        glyph: appWindow.icons.settings }
                ]

                delegate: Item {
                    id: navItem
                    required property var modelData
                    readonly property bool current: appWindow.navIndex === modelData.idx

                    width: parent.width
                    height: 44
                    y: modelData.idx * 50

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.controlRadius
                        color: (navHover.hovered && !navItem.current) ? Theme.sidebarSurfaceHi : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.tFast } }
                    }

                    HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 13

                        Glyph { appWindow: root.appWindow;
                            path: navItem.modelData.glyph
                            size: 19
                            color: navItem.current ? Theme.sidebarAccentSoft
                                 : navHover.hovered ? Theme.sidebarTxt : Theme.sidebarTxtFaint
                        }

                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr(navItem.modelData.key)
                            color: navItem.current ? Theme.sidebarTxt
                                 : navHover.hovered ? Theme.sidebarTxtDim : Theme.sidebarTxtFaint
                            font.pixelSize: 13
                            font.weight: navItem.current ? Font.DemiBold : Font.Normal
                            Behavior on color { ColorAnimation { duration: Theme.tFast } }
                        }
                    }

                    TapHandler { onTapped: appWindow.navIndex = navItem.modelData.idx }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Transport / daemon status
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            radius: Theme.controlRadius
            color: Theme.sidebarSurfaceSunk
            border.width: 1
            border.color: Theme.sidebarLine

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 13
                anchors.rightMargin: 13
                spacing: 9

                Glyph { appWindow: root.appWindow; path: appWindow.icons.bluetooth; size: 15; color: Theme.sidebarSuccess }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        textFormat: Text.PlainText
                        text: "SDK Core"
                        color: Theme.sidebarTxtDim
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: "IPC · RFCOMM V2"
                        color: Theme.sidebarTxtFaint
                        font.pixelSize: 10
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: Theme.sidebarSuccess
                }
            }
        }
    }
}
