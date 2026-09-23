import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts

// Slide-over from the right edge, behind the header's gear: everything the
// Overview leaves out, one line each. Values open their page; switches act
// in place. Always dark, like the sidebar, so it reads as a layer on top.
Item {
    id: panel
    required property var appWindow
    // Opened by the header's gear through appWindow.advancedOpen.
    readonly property bool open: appWindow.advancedOpen
    readonly property real panelWidth: Math.min(360, width - 40)

    // 0 closed, 1 open. The sheet's position is derived from it, so a
    // window resized while the panel is closed never animates the sheet in.
    property real shown: open ? 1 : 0
    Behavior on shown { NumberAnimation { duration: Theme.duration(380); easing.type: Easing.OutCubic } }

    visible: shown > 0
    function known(key) { var a = controller.featureStatus[key]; return controller.connected && a && a.availability === "valid" }
    function supported(key) { var a = controller.featureStatus[key]; return !a || a.availability !== "unsupported" }
    function close() { appWindow.advancedOpen = false }
    function go(index) { appWindow.navIndex = index; close() }

    // Scrim: dims the window and takes every press, hover and wheel meant
    // for the page under it; a click on it closes the panel.
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: panel.shown * (Theme.light ? 0.28 : 0.5)
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onClicked: panel.close()
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    Shortcut {
        sequence: "Esc"
        enabled: panel.open
        onActivated: panel.close()
    }

    Rectangle {
        id: sheet
        objectName: "advancedPanel"
        width: panel.panelWidth
        height: parent.height
        x: panel.width - width * panel.shown
        color: Theme.sidebarBg
        // Presses between the rows stay on the sheet.
        MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.AllButtons }

        Rectangle { width: 1; height: parent.height; color: Theme.sidebarLine }

        WheelScroll { flickable: flick }
        Flickable {
            id: flick
            anchors.fill: parent
            contentHeight: column.implicitHeight + 48
            boundsBehavior: Flickable.StopAtBounds
            // Wheel only: a drag is the slider's under the pointer.
            interactive: false
            clip: true
            ScrollBar.vertical: ScrollBar { policy: flick.contentHeight > flick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

            ColumnLayout {
                id: column
                x: 24
                y: 26
                width: flick.width - 48
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("advanced")
                            color: Theme.sidebarTxt
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("advanced_desc")
                            color: Theme.sidebarTxtDim
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                    IconButton {
                        objectName: "advancedClose"
                        appWindow: panel.appWindow
                        Layout.alignment: Qt.AlignTop
                        onSidebar: true
                        size: 32
                        glyphPath: "M6 6l12 12 M18 6L6 18"
                        toolTip: appWindow.tr("close")
                        onClicked: panel.close()
                    }
                }

                Divider { Layout.topMargin: 16 }

                // Sound
                Row_ {
                    appWindow: panel.appWindow
                    visible: controller.hasEqualizer
                    glyph: appWindow.icons.sliders
                    title: appWindow.tr("nav_equalizer")
                    value: controller.connected && controller.equalizerPreset >= 0 ? appWindow.trPreset(controller.equalizerPreset) : "—"
                    onClicked: panel.go(2)
                }
                Row_ {
                    appWindow: panel.appWindow
                    visible: controller.hasClearBass
                    glyph: appWindow.icons.waveform
                    title: appWindow.tr("clear_bass")
                    value: !controller.connected ? "—" : controller.clearBass > 0 ? "+" + controller.clearBass : String(controller.clearBass)
                    onClicked: panel.go(2)
                }
                Row_ {
                    appWindow: panel.appWindow
                    visible: controller.hasAmbient
                    glyph: appWindow.icons.ambient
                    title: appWindow.tr("ambient_level")
                    value: controller.ambientLevel > 0 ? String(controller.ambientLevel) : "—"
                    onClicked: panel.go(1)
                }

                Divider { visible: controller.hasDsee || controller.hasSpeakToChat || controller.hasAdaptiveVolume }

                Toggle_ {
                    appWindow: panel.appWindow
                    objectName: "advancedDsee"
                    visible: controller.hasDsee
                    glyph: appWindow.icons.sparkle
                    title: "DSEE Extreme"
                    checked: controller.dsee
                    ready: panel.known("dsee")
                    onToggled: function(on) { controller.setDsee(on) }
                }
                Toggle_ {
                    appWindow: panel.appWindow
                    visible: controller.hasSpeakToChat
                    glyph: appWindow.icons.chat
                    title: "Speak-to-Chat"
                    checked: controller.speakToChat
                    ready: panel.known("speakToChat")
                    onToggled: function(on) { controller.setSpeakToChat(on) }
                }
                Toggle_ {
                    appWindow: panel.appWindow
                    visible: controller.hasAdaptiveVolume
                    glyph: appWindow.icons.volume
                    title: "Adaptive Volume"
                    checked: controller.adaptiveVolume
                    ready: panel.known("adaptiveVolume")
                    onToggled: function(on) { controller.setAdaptiveVolume(on) }
                }

                Divider {}

                // Device
                Row_ {
                    appWindow: panel.appWindow
                    visible: panel.supported("autoPowerOff")
                    glyph: appWindow.icons.clock
                    title: appWindow.tr("auto_power_off")
                    value: !panel.known("autoPowerOff") ? "—"
                         : [appWindow.tr("apo_off"), appWindow.tr("apo_5min"), appWindow.tr("apo_15min"), appWindow.tr("apo_30min"),
                            appWindow.tr("apo_1h"), appWindow.tr("apo_3h")][controller.autoPowerOff] || "—"
                    onClicked: panel.go(3)
                }
                Row_ {
                    objectName: "advancedBatteryRow"
                    appWindow: panel.appWindow
                    glyph: appWindow.icons.batteryUp
                    title: appWindow.tr("nav_battery")
                    value: !controller.connected ? "—" : controller.isCharging ? appWindow.tr("charging")
                         : controller.batteryTimeLeft !== "" ? controller.batteryTimeLeft : "—"
                    onClicked: panel.go(5)
                }
                Row_ {
                    appWindow: panel.appWindow
                    glyph: appWindow.icons.swap
                    title: appWindow.tr("nav_device_switcher")
                    value: ""
                    onClicked: panel.go(4)
                }
                Row_ {
                    appWindow: panel.appWindow
                    objectName: "advancedPowerOff"
                    glyph: appWindow.icons.power
                    title: appWindow.tr("power_off")
                    value: ""
                    chevron: false
                    enabled: controller.connected
                    onClicked: { controller.powerOff(); panel.close() }
                }

                // Readout: what the link carries.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 16
                    implicitHeight: 72
                    radius: Theme.cardRadius
                    color: Theme.sidebarSurfaceSunk
                    border.width: 1
                    border.color: Theme.sidebarLine
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 16
                        Readout {
                            appWindow: panel.appWindow
                            objectName: "advancedCodec"
                            label: appWindow.tr("codec")
                            value: controller.connected && controller.codec.length && controller.codec !== "Unknown" ? controller.codec : "—"
                        }
                        Rectangle { width: 1; Layout.fillHeight: true; color: Theme.sidebarLine }
                        Readout {
                            appWindow: panel.appWindow
                            label: appWindow.tr("time_left")
                            value: !controller.connected || controller.isCharging || controller.batteryMinutesLeft < 0 ? "—"
                                 : Math.floor(controller.batteryMinutesLeft / 60) + ":" + ("0" + controller.batteryMinutesLeft % 60).slice(-2)
                        }
                    }
                }
            }
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.bottomMargin: 8
        height: 1
        color: Theme.sidebarLine
    }

    // A line that opens something: glyph, title, value, chevron.
    component Row_: Rectangle {
        id: line
        required property var appWindow
        property string glyph: ""
        property string title: ""
        property string value: ""
        property bool chevron: true
        signal clicked()
        Layout.fillWidth: true
        implicitHeight: 40
        radius: Theme.controlRadius
        color: lineHover.hovered && enabled ? Theme.sidebarSurface : "transparent"
        opacity: enabled ? 1 : 0.45
        Accessible.role: Accessible.Button
        Accessible.name: title
        HoverHandler { id: lineHover; enabled: line.enabled; cursorShape: Qt.PointingHandCursor }
        TapHandler { enabled: line.enabled; onTapped: line.clicked() }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 6
            spacing: 14
            Glyph { appWindow: line.appWindow; path: line.glyph; size: 18; weight: 1.6; color: Theme.sidebarTxt }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: line.title
                color: Theme.sidebarTxt
                font.pixelSize: 13
                elide: Text.ElideRight
            }
            Text {
                textFormat: Text.PlainText
                visible: text !== ""
                Layout.maximumWidth: 130
                text: line.value
                color: Theme.sidebarTxtDim
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            Glyph {
                appWindow: line.appWindow
                visible: line.chevron
                path: line.appWindow.icons.chevronRight
                size: 14
                color: Theme.sidebarTxtFaint
            }
        }
    }

    // A line with a switch in it.
    component Toggle_: RowLayout {
        id: toggle
        required property var appWindow
        property string glyph: ""
        property string title: ""
        property bool checked: false
        property bool ready: false
        signal toggled(bool on)
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 2
        Layout.preferredHeight: 40
        spacing: 14
        Glyph { appWindow: toggle.appWindow; path: toggle.glyph; size: 18; weight: 1.6; color: Theme.sidebarTxt }
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: toggle.title
            color: Theme.sidebarTxt
            font.pixelSize: 13
            elide: Text.ElideRight
        }
        Switch {
            id: sw
            checked: toggle.checked
            enabled: toggle.ready
            opacity: enabled ? 1 : 0.4
            implicitWidth: 42
            implicitHeight: 24
            Accessible.name: toggle.title
            onToggled: toggle.toggled(checked)
            Connections { target: toggle; function onCheckedChanged() { sw.checked = toggle.checked } }
            indicator: Rectangle {
                implicitWidth: 42
                implicitHeight: 24
                radius: height / 2
                color: sw.checked ? Theme.sidebarAccent : Theme.sidebarSurfaceHi
                border.width: 1
                border.color: sw.checked ? Theme.sidebarAccent : Theme.sidebarLineHi
                Behavior on color { ColorAnimation { duration: Theme.tBase } }
                Rectangle {
                    width: 16; height: 16; radius: 8
                    y: 4
                    x: sw.checked ? parent.width - width - 4 : 4
                    color: sw.checked ? Theme.sidebarBg : Theme.sidebarTxtDim
                    Behavior on x { NumberAnimation { duration: Theme.duration(200); easing.type: Easing.OutCubic } }
                }
            }
            contentItem: Item {}
        }
    }

    component Readout: ColumnLayout {
        id: readout
        required property var appWindow
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        spacing: 8
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: readout.label
            color: Theme.sidebarTxtDim
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        DotText { text: readout.value; dot: 2.4; maxWidth: readout.width; color: Theme.sidebarTxt }
    }
}
