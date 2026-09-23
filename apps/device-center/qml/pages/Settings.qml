import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../components"

ViewPage {
    id: root
    // More cards than fit the minimum window height, so this
    // page scrolls; the others still fit and don't.
    WheelScroll { flickable: settingsFlickItem }
    Flickable {
        id: settingsFlickItem
        objectName: "settingsFlick"
        anchors.fill: parent
        contentWidth: width
        contentHeight: settingsColumn.implicitHeight + 72
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Wheel only: a drag is the slider's under the pointer.
        interactive: false
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: settingsColumn
        x: appWindow.pageMargin; y: 22
        width: parent.width - 2 * appWindow.pageMargin
        spacing: 16

        SectionTitle { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: false
            eyebrow: appWindow.tr("settings_eyebrow")
            title: appWindow.tr("settings_title")
            subtitle: appWindow.tr("settings_subtitle")
        }

        // Card 1: System & Interface Preferences
        Card {
            appWindow: root.appWindow
            Layout.fillWidth: true
            implicitHeight: appearanceColumn.implicitHeight + 40
            ColumnLayout {
                id: appearanceColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 14
                Eyebrow { appWindow: root.appWindow; text: appWindow.tr("appearance") }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: appWindow.tr("icon_antialiasing"); color: Theme.txt; Layout.fillWidth: true; wrapMode: Text.Wrap }
                    NeoSwitch {
                        appWindow: root.appWindow
                        objectName: "iconSmoothingSwitch"
                        confirmedChecked: controller.iconAntialiasing
                        onToggled: controller.setIconAntialiasing(checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: appWindow.tr("theme"); color: Theme.txt; Layout.fillWidth: true }
                    RowLayout {
                        objectName: "themeSelector"
                        spacing: 6
                        Repeater {
                            model: [
                                { mode: "light",  label: appWindow.tr("theme_light") },
                                { mode: "dark",   label: appWindow.tr("theme_dark") },
                                { mode: "system", label: appWindow.tr("theme_system") }
                            ]
                            delegate: PillButton { appWindow: root.appWindow;
                                required property var modelData
                                compact: true
                                text: modelData.label
                                active: controller.themeMode === modelData.mode
                                onClicked: controller.setThemeMode(modelData.mode)
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: appWindow.tr("animations"); color: Theme.txt }
                        Text { text: appWindow.tr("animations_desc"); color: Theme.txtDim; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                    NeoSwitch {
                        appWindow: root.appWindow
                        objectName: "animationsSwitch"
                        confirmedChecked: controller.animationsEnabled
                        onToggled: controller.setAnimationsEnabled(checked)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: appWindow.tr("plain_font"); color: Theme.txt }
                        Text { text: appWindow.tr("plain_font_desc"); color: Theme.txtDim; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                    NeoSwitch {
                        appWindow: root.appWindow
                        objectName: "plainFontSwitch"
                        confirmedChecked: controller.plainFont
                        onToggled: controller.setPlainFont(checked)
                    }
                }
            }
        }

        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: systemColumn.implicitHeight + 44

            ColumnLayout {
                id: systemColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 22
                spacing: 16

                // Row 1: Init with OS
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        visible: !root.appWindow.compact
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.controlRadius
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: appWindow.icons.power
                            size: 18
                            color: Theme.accentSoft
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("init_with_os")
                            color: Theme.txt
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("init_with_os_desc")
                            color: Theme.txtDim
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                        }
                    }

                    NeoSwitch { appWindow: root.appWindow;
                        confirmedChecked: controller.autostart
                        onToggled: controller.setAutostart(checked)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.line
                    visible: trayAvailable
                }

                // Row: minimise to tray (only offered when a tray exists)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    visible: trayAvailable

                    Rectangle {
                        visible: !root.appWindow.compact
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.controlRadius
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: appWindow.icons.headphones
                            size: 18
                            color: Theme.accentSoft
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("minimize_to_tray")
                            color: Theme.txt
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("minimize_to_tray_desc")
                            color: Theme.txtDim
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                        }
                    }

                    NeoSwitch { appWindow: root.appWindow;
                        confirmedChecked: controller.minimizeToTray
                        onToggled: controller.setMinimizeToTray(checked)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.line
                }

                // Row 2: Language Selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        visible: !root.appWindow.compact
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.controlRadius
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)

                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: appWindow.icons.globe
                            size: 18
                            color: Theme.accentSoft
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("language")
                            color: Theme.txt
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("language_desc")
                            color: Theme.txtDim
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                        }
                    }

                    ComboBox {
                        id: langCombo
                        implicitWidth: 168
                        implicitHeight: 38
                        model: controller.availableLanguages
                        textRole: "name"
                        valueRole: "code"

                        currentIndex: {
                            var langs = controller.availableLanguages
                            for (var i = 0; i < langs.length; ++i) {
                                if (langs[i].code === controller.currentLanguage) return i
                            }
                            return 0
                        }

                        onActivated: {
                            var item = model[index]
                            if (item && item.code) {
                                controller.setLanguage(item.code)
                            }
                        }

                        background: Rectangle {
                            radius: Theme.controlRadius
                            color: langCombo.hovered ? Theme.surfaceHi : Theme.surfaceSunk
                            border.width: 1
                            border.color: langCombo.hovered ? Theme.lineHi : Theme.line
                            Behavior on color { ColorAnimation { duration: Theme.tFast } }
                        }

                        contentItem: Text {
                            textFormat: Text.PlainText
                            leftPadding: 14
                            rightPadding: 28
                            text: langCombo.displayText
                            color: Theme.txt
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        indicator: Glyph { appWindow: root.appWindow;
                            x: langCombo.width - width - 12
                            y: langCombo.height / 2 - height / 2
                            size: 14
                            color: Theme.txtFaint
                            path: appWindow.icons.chevron
                            rotation: langCombo.popup.visible ? 180 : 0
                            Behavior on rotation { NumberAnimation { duration: Theme.tBase } }
                        }

                        popup: Popup {
                            y: langCombo.height + 4
                            width: langCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight + 12, 260)
                            padding: 6
                            background: Rectangle {
                                radius: Theme.controlRadius
                                color: Theme.surface
                                border.width: 1
                                border.color: Theme.lineHi
                            }
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: langCombo.popup.visible ? langCombo.delegateModel : null
                                currentIndex: langCombo.highlightedIndex
                                ScrollIndicator.vertical: ScrollIndicator {}
                            }
                        }

                        delegate: ItemDelegate {
                            id: langDel
                            width: langCombo.width - 12
                            implicitHeight: 36
                            highlighted: langCombo.highlightedIndex === index
                            hoverEnabled: true

                            background: Rectangle {
                                radius: 8
                                color: langDel.hovered ? Theme.surfaceHi : "transparent"
                            }

                            contentItem: RowLayout {
                                spacing: 8
                                Text {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: (modelData.code === controller.currentLanguage) ? Theme.accentSoft : Theme.txt
                                    font.pixelSize: 13
                                    font.weight: (modelData.code === controller.currentLanguage) ? Font.DemiBold : Font.Normal
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Rectangle {
                                    visible: modelData.code === controller.currentLanguage
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: Theme.accent
                                }
                            }
                        }
                    }
                }
            }
        }

        // Card 2: Notifications (they go through the tray icon)
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: notifyColumn.implicitHeight + 44
            visible: trayAvailable

            ColumnLayout {
                id: notifyColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 22
                spacing: 16

                Eyebrow { appWindow: root.appWindow; text: appWindow.tr("notifications") }

                Repeater {
                    model: [
                        { key: "low", title: appWindow.tr("notify_low_battery_setting"), desc: appWindow.tr("notify_low_battery_setting_desc"), glyph: appWindow.icons.bolt },
                        { key: "conn", title: appWindow.tr("notify_connection_setting"), desc: appWindow.tr("notify_connection_setting_desc"), glyph: appWindow.icons.headphones },
                        { key: "charged", title: appWindow.tr("notify_charged_setting"), desc: appWindow.tr("notify_charged_setting_desc"), glyph: appWindow.icons.sparkle },
                        { key: "hotkey", title: appWindow.tr("notify_hotkey_setting"), desc: appWindow.tr("notify_hotkey_setting_desc"), glyph: appWindow.icons.keyboard },
                        { key: "update", title: appWindow.tr("notify_update_setting"), desc: appWindow.tr("notify_update_setting_desc"), glyph: appWindow.icons.download }
                    ]
                    delegate: RowLayout {
                        id: notifyRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 16

                        Rectangle {
                            visible: !root.appWindow.compact
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: Theme.controlRadius
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                            Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: notifyRow.modelData.glyph; size: 18; color: Theme.accentSoft }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: notifyRow.modelData.title
                                color: Theme.txt
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: notifyRow.modelData.desc
                                color: Theme.txtDim
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                            }
                        }

                        // Threshold picker sits only on the low-battery row.
                        ComboBox {
                            id: thresholdCombo
                            visible: notifyRow.modelData.key === "low"
                            implicitWidth: 92
                            implicitHeight: 38
                            model: [10, 15, 20, 25, 30]
                            currentIndex: Math.max(0, model.indexOf(controller.lowBatteryThreshold))
                            onActivated: controller.setLowBatteryThreshold(model[index])
                            displayText: currentText + "%"

                            background: Rectangle {
                                radius: Theme.controlRadius
                                color: thresholdCombo.hovered ? Theme.surfaceHi : Theme.surfaceSunk
                                border.width: 1
                                border.color: thresholdCombo.hovered ? Theme.lineHi : Theme.line
                            }
                            contentItem: Text {
                                textFormat: Text.PlainText
                                leftPadding: 13
                                rightPadding: 28
                                text: thresholdCombo.displayText
                                color: Theme.txt
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                            }
                            indicator: Glyph { appWindow: root.appWindow;
                                x: thresholdCombo.width - width - 12
                                y: thresholdCombo.height / 2 - height / 2
                                size: 14
                                color: Theme.txtFaint
                                path: appWindow.icons.chevron
                                rotation: thresholdCombo.popup.visible ? 180 : 0
                            }
                        }

                        NeoSwitch { appWindow: root.appWindow;
                            objectName: "notifySwitch-" + notifyRow.modelData.key
                            confirmedChecked: notifyRow.modelData.key === "low" ? controller.notifyLowBattery
                                            : notifyRow.modelData.key === "conn" ? controller.notifyConnection
                                            : notifyRow.modelData.key === "hotkey" ? controller.notifyHotkeys
                                            : notifyRow.modelData.key === "update" ? controller.notifyUpdates
                                            : controller.notifyCharged
                            onToggled: {
                                if (notifyRow.modelData.key === "low") controller.setNotifyLowBattery(checked)
                                else if (notifyRow.modelData.key === "conn") controller.setNotifyConnection(checked)
                                else if (notifyRow.modelData.key === "hotkey") controller.setNotifyHotkeys(checked)
                                else if (notifyRow.modelData.key === "update") controller.setNotifyUpdates(checked)
                                else controller.setNotifyCharged(checked)
                            }
                        }
                    }
                }
            }
        }

        // Card 3: Global hotkeys. The action list is static so the delegates
        // (and the focused capture field) survive a bindings update; each row
        // looks its own binding up by action.
        Card { appWindow: root.appWindow;
            objectName: "hotkeysCard"
            Layout.fillWidth: true
            Layout.preferredHeight: hotkeyColumn.implicitHeight + 44

            ColumnLayout {
                id: hotkeyColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 22
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Eyebrow { appWindow: root.appWindow; text: appWindow.tr("hotkeys") }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: hotkeys.supported ? appWindow.tr("hotkeys_desc") : appWindow.tr("hotkeys_unavailable")
                        color: hotkeys.supported ? Theme.txtDim : Theme.danger
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                Repeater {
                    model: [
                        { action: "toggleNoiseControl", title: appWindow.tr("hotkey_toggle_noise"), glyph: appWindow.icons.shield },
                        { action: "noiseControlOff", title: appWindow.tr("hotkey_off"), glyph: appWindow.icons.power },
                        { action: "toggleSpeakToChat", title: appWindow.tr("hotkey_speak_to_chat"), glyph: appWindow.icons.chat },
                        { action: "showWindow", title: appWindow.tr("hotkey_show_window"), glyph: appWindow.icons.home }
                    ]
                    // Narrow: the action on its own line, the field and the
                    // switch under it.
                    delegate: GridLayout {
                        id: hotkeyRow
                        required property var modelData
                        readonly property var binding: hotkeys.bindings.find(b => b.action === hotkeyRow.modelData.action)
                        readonly property bool conflict: binding.status === "conflict"
                        Layout.fillWidth: true
                        columns: root.appWindow.compact ? 2 : 4
                        columnSpacing: 16
                        rowSpacing: 8

                        Rectangle {
                            visible: !root.appWindow.compact
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: Theme.controlRadius
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                            border.width: 1
                            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                            Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: hotkeyRow.modelData.glyph; size: 18; color: Theme.accentSoft }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: root.appWindow.compact ? 2 : 1
                            spacing: 3
                            Text {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: hotkeyRow.modelData.title
                                color: Theme.txt
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                            }
                            Text {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                visible: hotkeyRow.conflict
                                text: appWindow.tr("hotkey_conflict")
                                color: Theme.danger
                                font.pixelSize: 12
                            }
                        }

                        // Capture field: click, press the combination, done.
                        // Backspace clears, Escape backs out.
                        Rectangle {
                            id: captureField
                            objectName: "hotkeyCapture-" + hotkeyRow.modelData.action
                            Layout.preferredWidth: 168
                            Layout.fillWidth: root.appWindow.compact
                            Layout.preferredHeight: 38
                            radius: Theme.controlRadius
                            color: captureArea.containsMouse && !activeFocus ? Theme.surfaceHi : Theme.surfaceSunk
                            border.width: 1
                            border.color: activeFocus ? Theme.accent : hotkeyRow.conflict ? Theme.danger
                                        : captureArea.containsMouse ? Theme.lineHi : Theme.line
                            activeFocusOnTab: true
                            enabled: hotkeys.supported
                            opacity: enabled ? 1 : 0.5
                            Behavior on color { ColorAnimation { duration: Theme.tFast } }
                            Behavior on border.color { ColorAnimation { duration: Theme.tFast } }

                            // Registered combinations would fire instead of
                            // reaching this field, so they are let go while
                            // it listens.
                            onActiveFocusChanged: hotkeys.suspend(activeFocus)
                            readonly property bool windowActive: Window.active
                            onWindowActiveChanged: if (!windowActive) focus = false
                            Component.onDestruction: if (activeFocus) hotkeys.suspend(false)

                            Keys.onPressed: (event) => {
                                event.accepted = true
                                if (event.key === Qt.Key_Escape) { captureField.focus = false; return }
                                if ((event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) && event.modifiers === Qt.NoModifier) {
                                    hotkeys.setShortcut(hotkeyRow.modelData.action, "")
                                    captureField.focus = false
                                    return
                                }
                                var sequence = hotkeys.sequenceFromKey(event.key, event.modifiers)
                                if (sequence === "") return
                                hotkeys.setShortcut(hotkeyRow.modelData.action, sequence)
                                captureField.focus = false
                            }

                            MouseArea {
                                id: captureArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: captureField.forceActiveFocus()
                            }

                            Text {
                                textFormat: Text.PlainText
                                anchors.centerIn: parent
                                width: parent.width - 24
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                text: captureField.activeFocus ? appWindow.tr("hotkey_capture_hint")
                                    : hotkeyRow.binding.display !== "" ? hotkeyRow.binding.display
                                    : appWindow.tr("hotkey_not_set")
                                color: captureField.activeFocus || hotkeyRow.binding.display === "" ? Theme.txtDim : Theme.txt
                                font.pixelSize: 12
                                font.weight: hotkeyRow.binding.display !== "" && !captureField.activeFocus ? Font.DemiBold : Font.Normal
                            }
                        }

                        NeoSwitch { appWindow: root.appWindow;
                            objectName: "hotkeySwitch-" + hotkeyRow.modelData.action
                            enabled: hotkeys.supported
                            opacity: enabled ? 1 : 0.5
                            confirmedChecked: hotkeyRow.binding.enabled
                            onToggled: hotkeys.setEnabled(hotkeyRow.modelData.action, checked)
                        }
                    }
                }
            }
        }

        // Card 4: the Device Hub and the tray. Only where there is a tray to
        // put the hub on.
        Card { appWindow: root.appWindow;
            objectName: "hubCard"
            visible: trayAvailable
            Layout.fillWidth: true
            Layout.preferredHeight: hubColumn.implicitHeight + 44

            // Pinned to the top rather than filling the card: the card's
            // height is read from this column, and with wrapped rows the
            // round trip took a third layout pass (Qt aborts at two).
            ColumnLayout {
                id: hubColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 22
                spacing: 16

                Eyebrow { appWindow: root.appWindow; text: appWindow.tr("hub_card_title") }

                // All Bluetooth devices, or Sony only.
                HubSettingRow {
                    title: appWindow.tr("hub_show_system")
                    desc: peripherals.systemSourceAvailable ? appWindow.tr("hub_show_system_desc") : appWindow.tr("hub_show_system_unavailable")
                    glyph: appWindow.icons.bluetooth
                    NeoSwitch {
                        appWindow: root.appWindow
                        objectName: "hubShowSystemSwitch"
                        enabled: peripherals.systemSourceAvailable
                        confirmedChecked: hubSettings.showSystemDevices && peripherals.systemSourceAvailable
                        onToggled: hubSettings.showSystemDevices = checked
                    }
                }

                // How often the OS list is re-read.
                HubSettingRow {
                    visible: peripherals.systemSourceAvailable
                    title: appWindow.tr("hub_poll_interval")
                    desc: appWindow.tr("hub_poll_interval_desc")
                    glyph: appWindow.icons.refresh
                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: [15, 30, 60, 120]
                            delegate: PillButton { appWindow: root.appWindow;
                                required property int modelData
                                compact: true
                                text: modelData < 60 ? appWindow.tr("seconds_short").arg(modelData) : appWindow.tr("duration_minutes").arg(modelData / 60)
                                active: hubSettings.pollIntervalSeconds === modelData
                                onClicked: hubSettings.pollIntervalSeconds = modelData
                            }
                        }
                    }
                }

                // What the tray icon's left click opens.
                HubSettingRow {
                    title: appWindow.tr("hub_tray_click")
                    desc: appWindow.tr("hub_tray_click_desc")
                    glyph: appWindow.icons.home
                    RowLayout {
                        objectName: "hubTrayClickSelector"
                        spacing: 6
                        Repeater {
                            model: [
                                { action: "hub",    label: appWindow.tr("hub_card_title") },
                                { action: "window", label: appWindow.tr("main_window") }
                            ]
                            delegate: PillButton { appWindow: root.appWindow;
                                required property var modelData
                                compact: true
                                text: modelData.label
                                active: hubSettings.trayClickAction === modelData.action
                                onClicked: hubSettings.trayClickAction = modelData.action
                            }
                        }
                    }
                }

                // One icon, or one per chosen device.
                HubSettingRow {
                    title: appWindow.tr("hub_tray_mode")
                    desc: appWindow.tr("hub_tray_mode_desc")
                    glyph: appWindow.icons.battery
                    RowLayout {
                        objectName: "hubTrayModeSelector"
                        spacing: 6
                        Repeater {
                            model: [
                                { mode: "single",    label: appWindow.tr("hub_tray_mode_single") },
                                { mode: "perDevice", label: appWindow.tr("hub_tray_mode_per_device") }
                            ]
                            delegate: PillButton { appWindow: root.appWindow;
                                required property var modelData
                                compact: true
                                text: modelData.label
                                active: hubSettings.trayMode === modelData.mode
                                onClicked: hubSettings.trayMode = modelData.mode
                            }
                        }
                    }
                }

                // Per-device choice, and the honest word about where Windows
                // puts new tray icons.
                ColumnLayout {
                    visible: hubSettings.trayMode === "perDevice"
                    Layout.fillWidth: true
                    Layout.leftMargin: 54
                    spacing: 10
                    Repeater {
                        model: peripherals
                        delegate: RowLayout {
                            id: trayDeviceRow
                            required property string address
                            required property string name
                            required property string kind
                            required property bool connected
                            Layout.fillWidth: true
                            spacing: 12
                            Glyph { appWindow: root.appWindow; path: appWindow.icons[trayDeviceRow.kind === "other" ? "bluetooth" : trayDeviceRow.kind]; size: 16; weight: 1.6
                                    color: trayDeviceRow.connected ? Theme.txt : Theme.txtFaint }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: trayDeviceRow.name
                                color: trayDeviceRow.connected ? Theme.txt : Theme.txtDim
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            NeoSwitch {
                                appWindow: root.appWindow
                                objectName: "trayDeviceSwitch-" + trayDeviceRow.address
                                confirmedChecked: hubSettings.trayDevices.indexOf(trayDeviceRow.address) !== -1
                                onToggled: hubSettings.setInTray(trayDeviceRow.address, checked)
                            }
                        }
                    }
                    Text {
                        visible: peripherals.count === 0
                        textFormat: Text.PlainText
                        text: appWindow.tr("hub_tray_no_devices")
                        color: Theme.txtFaint
                        font.pixelSize: 12
                    }
                    Text {
                        visible: Qt.platform.os === "windows"
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: appWindow.tr("hub_tray_overflow_note")
                        color: Theme.txtFaint
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        // Split Cards: About Application & Community/Donate, stacked when
        // the window is narrow.
        GridLayout {
            Layout.fillWidth: true
            columns: appWindow.stacked ? 1 : 2
            columnSpacing: 18
            rowSpacing: 16

            // Left Card: About App & Version, and the update check.
            Card { appWindow: root.appWindow;
                objectName: "aboutCard"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: aboutColumn.implicitHeight + 44

                ColumnLayout {
                    id: aboutColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 22
                    spacing: 14

                    RowLayout {
                        spacing: 14
                        BrandTile { appWindow: root.appWindow;
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                textFormat: Text.PlainText
                                text: appWindow.tr("about_app")
                                color: Theme.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: "Sony Device Center"
                                color: Theme.txtDim
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: verLabel.implicitWidth + 20
                            radius: Theme.cardRadius
                            color: Theme.surfaceSunk
                            border.width: 1
                            border.color: Theme.lineHi

                            Text {
                                textFormat: Text.PlainText
                                id: verLabel
                                anchors.centerIn: parent
                                text: "v" + controller.appVersion
                                color: Theme.accentSoft
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.line
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: 2
                            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("protocol_core") }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: "MDR V1 & V2, C++20"
                                color: Theme.txt
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: 2
                            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("framework") }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: "Qt 6 Quick / QML"
                                color: Theme.txt
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: 2
                            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("license") }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: appWindow.tr("license_value")
                                color: Theme.txt
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.line
                    }

                    // What GitHub said last, and the way to act on it: the
                    // installer for this platform and the release notes when
                    // there is something newer, a re-check otherwise.
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Eyebrow { appWindow: root.appWindow; text: appWindow.tr("updates") }
                        Text {
                            objectName: "updateStatus"
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: updates.state === "available" ? appWindow.tr("update_available").arg(updates.latestVersion)
                                : updates.state === "checking" ? appWindow.tr("update_checking")
                                : updates.state === "upToDate" ? appWindow.tr("update_up_to_date")
                                : updates.state === "failed" ? appWindow.tr("update_failed")
                                : appWindow.tr("update_idle")
                            color: updates.state === "available" ? Theme.txt : Theme.txtDim
                            font.pixelSize: 12
                            font.weight: updates.state === "available" ? Font.DemiBold : Font.Medium
                            wrapMode: Text.Wrap
                        }
                    }
                    // The actions on their own line: two buttons beside the
                    // status would not fit the card at the minimum width.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        PillButton { appWindow: root.appWindow;
                            objectName: "updateDownload"
                            visible: updates.available && updates.downloadUrl !== ""
                            compact: true
                            active: true
                            text: appWindow.tr("update_download")
                            glyphPath: appWindow.icons.download
                            onClicked: controller.openUrl(updates.downloadUrl)
                        }
                        PillButton { appWindow: root.appWindow;
                            objectName: "updateReleasePage"
                            visible: updates.available
                            compact: true
                            text: appWindow.tr("update_release_page")
                            glyphPath: appWindow.icons.externalLink
                            onClicked: controller.openUrl(updates.releaseUrl)
                        }
                        PillButton { appWindow: root.appWindow;
                            objectName: "updateCheckNow"
                            visible: !updates.available
                            enabled: !updates.checking
                            compact: true
                            text: appWindow.tr("update_check_now")
                            glyphPath: appWindow.icons.refresh
                            onClicked: updates.check()
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("update_check_on_start")
                            color: Theme.txtDim
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                        NeoSwitch { appWindow: root.appWindow;
                            objectName: "updateCheckOnStartSwitch"
                            confirmedChecked: controller.checkUpdatesOnStart
                            onToggled: controller.setCheckUpdatesOnStart(checked)
                        }
                    }
                }
            }

            // Right Card: GitHub & Donate
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 184

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 14

                    RowLayout {
                        spacing: 14
                        Rectangle {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                            radius: Theme.controlRadius
                            color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.16)
                            border.width: 1
                            border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.45)

                            Glyph { appWindow: root.appWindow;
                                anchors.centerIn: parent
                                path: appWindow.icons.heart
                                size: 20
                                color: Theme.danger
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                textFormat: Text.PlainText
                                text: appWindow.tr("links_support")
                                color: Theme.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: appWindow.tr("github_sponsorship")
                                color: Theme.txtDim
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                            }
                        }
                    }

                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: appWindow.tr("donate_desc")
                        color: Theme.txtDim
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        spacing: 10

                        PillButton { appWindow: root.appWindow;
                            compact: true
                            glyphPath: appWindow.icons.github
                            text: appWindow.tr("btn_github")
                            onClicked: controller.openUrl("https://github.com/marconvcm/sony-device-center")
                        }

                        PillButton { appWindow: root.appWindow;
                            compact: true
                            tint: Theme.danger
                            glyphPath: appWindow.icons.heart
                            text: appWindow.tr("btn_donate")
                            onClicked: controller.openUrl("https://github.com/sponsors/marconvcm")
                        }
                    }
                }
            }
        }

        // Independent-project notice. It lives on the page rather
        // than inside the About card because it is a trademark
        // statement, not a detail about the build.
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.preferredHeight: 1
            color: Theme.line
        }

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: appWindow.tr("disclaimer")
            color: Theme.txtFaint
            font.pixelSize: 12
            lineHeight: 1.4
            wrapMode: Text.WordWrap
        }

    }
    }

    // One hub setting: glyph tile, title and description, control on the
    // right; in a narrow window a wide control goes under the text.
    component HubSettingRow: GridLayout {
        id: settingRow
        property string title: ""
        property string desc: ""
        property string glyph: ""
        default property alias control: controlSlot.data
        readonly property bool stacked: root.appWindow.compact && controlSlot.implicitWidth > 140
        Layout.fillWidth: true
        columns: stacked ? 1 : 3
        columnSpacing: 16
        rowSpacing: 10
        Rectangle {
            visible: !root.appWindow.compact
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            Layout.alignment: Qt.AlignTop
            radius: Theme.controlRadius
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
            border.width: 1
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
            Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: settingRow.glyph; size: 18; color: Theme.accentSoft }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: settingRow.title
                color: Theme.txt
                font.pixelSize: 14
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.preferredWidth: 1
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: settingRow.desc !== ""
                text: settingRow.desc
                color: Theme.txtDim
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.preferredWidth: 1
            }
        }
        Item {
            id: controlSlot
            Layout.alignment: settingRow.stacked ? Qt.AlignLeft : Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
