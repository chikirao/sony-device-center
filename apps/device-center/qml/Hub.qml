import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

// The Device Hub: a tool window that sits by the tray icon and lists every
// Bluetooth device the OS and the controller know about, one 56 px row
// each, with the connected Sony set's quick controls inline. It shares the
// main window's engine, so `controller`, `peripherals` and Theme are the
// same objects the main window uses; `mainWindow` is handed in as a context
// property so the shared components get their appWindow (icons, tr).
Window {
    id: hub
    readonly property var appWindow: mainWindow
    readonly property int rowHeight: 56
    readonly property int minRows: 3
    readonly property int maxRows: 6
    readonly property int footerHeight: 46
    // Three rows of space even when there is less to show, six at most; the
    // list scrolls beyond that.
    readonly property int visibleRows: Math.max(minRows, Math.min(maxRows, peripherals.count))
    // Set once the window has actually held focus this time round: losing
    // focus only counts as "clicked elsewhere" after that, otherwise a hub
    // opened while another window is busy would close itself at once.
    property bool wasActive: false
    // Hover tint that reads on both papers: surfaceHi is white-on-white in
    // the light theme, so that one sinks instead.
    readonly property color hoverColor: Theme.light ? Theme.surfaceSunk : Theme.surfaceHi

    // A row or the footer asked for the main window; -1 keeps its page.
    signal mainWindowRequested(int page)

    width: 360
    // The extra 8 px below the card is where the slide-in comes from.
    height: card.implicitHeight + 8
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    title: appWindow.tr("hub_title")
    visible: false

    function present() {
        wasActive = false
        exit.stop()
        if (Theme.motionEnabled) { card.opacity = 0; card.y = 8; enter.restart() }
        else { card.opacity = 1; card.y = 0 }
        visible = true
        requestActivate()
        card.forceActiveFocus()
    }
    function dismiss() {
        if (!visible) return
        enter.stop()
        if (Theme.motionEnabled) exit.restart()
        else visible = false
    }

    onActiveChanged: {
        if (active) wasActive = true
        else if (visible && wasActive) dismiss()
    }

    ParallelAnimation {
        id: enter
        NumberAnimation { target: card; property: "opacity"; to: 1; duration: Theme.tBase; easing.type: Easing.OutQuad }
        NumberAnimation { target: card; property: "y"; to: 0; duration: Theme.tSlow; easing.type: Easing.OutCubic }
    }
    SequentialAnimation {
        id: exit
        ParallelAnimation {
            NumberAnimation { target: card; property: "opacity"; to: 0; duration: Theme.tFast; easing.type: Easing.InQuad }
            NumberAnimation { target: card; property: "y"; to: 6; duration: Theme.tFast; easing.type: Easing.InQuad }
        }
        ScriptAction { script: hub.visible = false }
    }

    function statusText(sony, active, connected, codec, noiseMode) {
        if (!connected) return appWindow.tr("hub_not_connected")
        var parts = [appWindow.tr("connected")]
        if (sony && active) {
            if (codec.length) parts.push(codec)
            if (noiseMode === "cancelling") parts.push(appWindow.tr("mode_anc"))
            else if (noiseMode === "ambient") parts.push(appWindow.tr("mode_ambient"))
            else if (noiseMode === "off") parts.push(appWindow.tr("mode_off"))
        }
        return parts.join(" · ")
    }
    function glyphFor(kind) {
        if (kind === "headphones") return appWindow.icons.headphones
        if (kind === "earbuds") return appWindow.icons.earbuds
        if (kind === "mouse") return appWindow.icons.mouse
        if (kind === "keyboard") return appWindow.icons.keyboard
        if (kind === "gamepad") return appWindow.icons.gamepad
        return appWindow.icons.bluetooth
    }

    Rectangle {
        id: card
        width: hub.width
        implicitHeight: hub.visibleRows * hub.rowHeight + 1 + hub.footerHeight + 2
        height: implicitHeight
        radius: Theme.cardRadius
        color: Theme.surface
        border.width: 1
        border.color: Theme.lineHi
        clip: true
        // Esc closes; the card holds focus while the hub is up.
        focus: true
        Keys.onEscapePressed: hub.dismiss()

        ListView {
            id: list
            objectName: "hubList"
            x: 1; y: 1
            width: parent.width - 2
            height: hub.visibleRows * hub.rowHeight
            clip: true
            model: peripherals
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: HubRow {}
            // Row moves (a device connecting) slide rather than jump.
            move: Transition { NumberAnimation { properties: "y"; duration: Theme.tBase; easing.type: Easing.OutCubic } }
            displaced: Transition { NumberAnimation { properties: "y"; duration: Theme.tBase; easing.type: Easing.OutCubic } }
        }

        // Nothing paired at all.
        ColumnLayout {
            objectName: "hubEmpty"
            visible: peripherals.count === 0
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 30
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            spacing: 4
            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                Layout.bottomMargin: 8
                radius: Theme.controlRadius
                color: Theme.surfaceSunk
                border.width: 1
                border.color: Theme.line
                Glyph { appWindow: hub.appWindow; anchors.centerIn: parent; path: appWindow.icons.bluetooth; size: 20; color: Theme.txtFaint }
            }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("hub_empty_title")
                color: Theme.txt
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: appWindow.tr("hub_empty_desc")
                color: Theme.txtDim
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }

        Rectangle { x: 1; y: list.y + list.height; width: parent.width - 2; height: 1; color: Theme.line }

        // Footer: the way into the main window, and straight to Settings.
        Item {
            id: footer
            x: 1
            y: list.y + list.height + 1
            width: parent.width - 2
            height: hub.footerHeight
            Rectangle {
                id: openButton
                objectName: "hubOpenMain"
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 6
                width: openRow.implicitWidth + 20
                radius: Theme.controlRadius
                color: openHover.hovered ? hub.hoverColor : "transparent"
                HoverHandler { id: openHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: hub.mainWindowRequested(-1) }
                RowLayout {
                    id: openRow
                    anchors.centerIn: parent
                    spacing: 8
                    BrandMark { appWindow: hub.appWindow; size: 14; color: Theme.txt }
                    Text {
                        textFormat: Text.PlainText
                        text: appWindow.tr("tray_show")
                        color: Theme.txt
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                }
            }
            Rectangle {
                objectName: "hubSettings"
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 6
                width: height
                radius: Theme.controlRadius
                color: settingsHover.hovered ? hub.hoverColor : "transparent"
                HoverHandler { id: settingsHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: hub.mainWindowRequested(6) }
                Glyph { appWindow: hub.appWindow; anchors.centerIn: parent; path: appWindow.icons.settings; size: 17; weight: 1.6
                        color: settingsHover.hovered ? Theme.txt : Theme.txtDim }
            }
        }
    }

    // One device. Sony rows open the main window; the connected Sony set
    // also gets its noise-control segment and power inline.
    component HubRow: Item {
        id: row
        required property int index
        required property string address
        required property string name
        required property string kind
        required property bool connected
        required property int battery
        required property int batteryLeft
        required property int batteryRight
        required property int batteryCase
        required property bool hasDualBattery
        required property bool sony
        required property bool active
        required property bool charging
        required property string codec
        required property string noiseMode
        readonly property bool live: sony && active && connected && controller.connected
        readonly property color inkFor: connected ? Theme.txt : Theme.txtFaint

        width: ListView.view ? ListView.view.width : hub.width
        height: hub.rowHeight

        // Hover is instant, no fade: a quick panel should feel like a menu.
        // The top row's highlight follows the card's rounded corners; the
        // card's own clip is rectangular and would let it poke out.
        Rectangle {
            objectName: "hubRowBg"
            anchors.fill: parent
            topLeftRadius: row.index === 0 ? Theme.cardRadius - 1 : 0
            topRightRadius: row.index === 0 ? Theme.cardRadius - 1 : 0
            color: rowHover.hovered && row.sony && !quickControls.hovered ? hub.hoverColor : "transparent"
        }
        HoverHandler { id: rowHover; cursorShape: row.sony ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler {
            enabled: row.sony
            // A tap on the quick controls is theirs alone; only the rest of
            // the row opens the main window.
            onTapped: function(point) {
                if (quickControls.visible && quickControls.contains(quickControls.mapFromItem(row, point.position))) return
                if (pin.visible && pin.contains(pin.mapFromItem(row, point.position))) return
                hub.mainWindowRequested(row.active ? 0 : 4)
            }
        }

        // Class tile
        Rectangle {
            id: tile
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 36; height: 36
            radius: Theme.controlRadius
            color: row.live ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14) : Theme.surfaceSunk
            border.width: 1
            border.color: row.live ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4) : Theme.line
            Glyph { appWindow: hub.appWindow; anchors.centerIn: parent; path: hub.glyphFor(row.kind); size: 18; weight: 1.6
                    color: row.connected ? Theme.txt : Theme.txtFaint }
        }

        // "Show in tray", only while the tray is in per-device mode: shown
        // on hover, and always while the device is pinned.
        Rectangle {
            id: pin
            objectName: "hubPin"
            readonly property bool pinned: hubSettings.trayDevices.indexOf(row.address) !== -1
            visible: hubSettings.trayMode === "perDevice" && (pinned || rowHover.hovered)
            anchors.right: rightBlock.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 22; height: 22
            radius: 6
            color: pinned ? Theme.accent : pinHover.hovered ? hub.hoverColor : "transparent"
            border.width: 1
            border.color: pinned ? Theme.accent : pinHover.hovered ? Theme.lineHi : Theme.line
            HoverHandler { id: pinHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: hubSettings.setInTray(row.address, !pin.pinned) }
            Glyph { appWindow: hub.appWindow; anchors.centerIn: parent; path: appWindow.icons.pin; size: 13; weight: 1.8
                    color: pin.pinned ? Theme.accentText : Theme.txtDim }
        }

        // Name and status; elides against whatever the right block needs.
        ColumnLayout {
            anchors.left: tile.right
            anchors.leftMargin: 12
            anchors.right: pin.visible ? pin.left : rightBlock.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: row.name
                color: row.inkFor
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Rectangle {
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    radius: 3
                    color: row.connected ? Theme.success : Theme.txtFaint
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: hub.statusText(row.sony, row.active, row.connected, row.codec, row.noiseMode)
                    color: row.connected ? Theme.txtDim : Theme.txtFaint
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }

        // Charge on top, the Sony quick controls under it when there are any.
        ColumnLayout {
            id: rightBlock
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            // Earbuds: the two sides and the case, each behind its letter.
            // No percent signs: three of them would eat the name column.
            RowLayout {
                visible: row.hasDualBattery
                Layout.alignment: Qt.AlignRight
                spacing: 9
                Repeater {
                    model: [
                        { label: appWindow.tr("battery_left_short"),  value: row.batteryLeft },
                        { label: appWindow.tr("battery_right_short"), value: row.batteryRight },
                        { label: appWindow.tr("battery_case"),        value: row.batteryCase }
                    ]
                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        visible: modelData.value >= 0
                        spacing: 3
                        Text {
                            Layout.alignment: Qt.AlignVCenter
                            textFormat: Text.PlainText
                            text: modelData.label
                            color: Theme.txtFaint
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                        }
                        DotText {
                            Layout.alignment: Qt.AlignVCenter
                            text: String(modelData.value)
                            dot: row.live ? 1.6 : 1.9
                            delay: index * 80
                            color: !row.connected ? Theme.txtFaint : modelData.value <= 20 ? Theme.danger : Theme.txt
                        }
                    }
                }
            }
            // Everything else: one number, and nothing at all when the OS
            // has no reading (a speaker, an idle mouse).
            RowLayout {
                visible: !row.hasDualBattery && row.battery >= 0
                Layout.alignment: Qt.AlignRight
                spacing: 4
                Glyph { appWindow: hub.appWindow; visible: row.charging; path: appWindow.icons.bolt; size: 12; weight: 1.8; color: Theme.txtDim }
                DotText {
                    objectName: "hubBattery"
                    text: row.battery + "%"
                    // Smaller when the quick controls share the row's height.
                    dot: row.live ? 2.0 : 2.2
                    color: !row.connected ? Theme.txtFaint : row.battery <= 20 ? Theme.danger : Theme.txt
                }
            }

            // NC / Ambient / Off and power, for the set we are talking to.
            RowLayout {
                id: quickControls
                readonly property bool hovered: controlsHover.hovered
                visible: row.live
                Layout.alignment: Qt.AlignRight
                spacing: 4
                HoverHandler { id: controlsHover }
                Row {
                    id: segment
                    spacing: 2
                    Repeater {
                        model: [
                            { mode: "cancelling", glyph: appWindow.icons.shield,   shown: controller.hasAnc },
                            { mode: "ambient",    glyph: appWindow.icons.ambient,  shown: controller.hasAmbient },
                            { mode: "off",        glyph: appWindow.icons.noiseOff, shown: true }
                        ]
                        delegate: Rectangle {
                            id: seg
                            required property var modelData
                            objectName: "hubMode-" + modelData.mode
                            readonly property bool current: controller.noiseControlMode === modelData.mode
                            visible: modelData.shown
                            width: 26; height: 20
                            radius: 6
                            color: current ? Theme.accent : segHover.hovered ? hub.hoverColor : Theme.surfaceSunk
                            border.width: 1
                            border.color: current ? Theme.accent : segHover.hovered ? Theme.lineHi : Theme.line
                            opacity: controller.busy ? 0.6 : 1
                            HoverHandler { id: segHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                enabled: !controller.busy
                                onTapped: {
                                    if (seg.modelData.mode === "cancelling") controller.setAnc(true)
                                    else if (seg.modelData.mode === "ambient") controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
                                    else controller.setNoiseControlOff()
                                }
                            }
                            Glyph { appWindow: hub.appWindow; anchors.centerIn: parent; path: seg.modelData.glyph; size: 13; weight: 1.8
                                    color: seg.current ? Theme.accentText : Theme.txtDim }
                        }
                    }
                }
                Rectangle {
                    objectName: "hubPower"
                    width: 26; height: 20
                    radius: 6
                    color: powerHover.hovered ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14) : "transparent"
                    border.width: 1
                    border.color: powerHover.hovered ? Theme.danger : Theme.line
                    opacity: controller.busy ? 0.6 : 1
                    HoverHandler { id: powerHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { enabled: !controller.busy; onTapped: controller.powerOff() }
                    Glyph { appWindow: hub.appWindow; anchors.centerIn: parent; path: appWindow.icons.power; size: 13; weight: 1.8
                            color: powerHover.hovered ? Theme.danger : Theme.txtDim }
                }
            }
        }

        Rectangle {
            visible: row.index < peripherals.count - 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 60
            height: 1
            color: Theme.line
        }
    }
}
