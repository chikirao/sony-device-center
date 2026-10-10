import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ViewPage {
    id: root

    function availability(key) { return controller.featureStatus[key] }
    function known(key) { var a = availability(key); return controller.connected && a && a.availability === "valid" }
    // A connected model without the feature keeps its card, greyed and
    // locked, so the page reads the same on every model.
    function lacks(has) { return controller.connected && !has }

    // A Speak-to-Chat setting as a row: its label on the left, the dropdown
    // on the right at the width the card gives it. Until the settings are
    // read, a placeholder of the same size stands in for the dropdown.
    component StcPicker: RowLayout {
        id: picker
        required property var appWindow
        property bool live: false
        property bool loading: false
        property real controlWidth: 240
        readonly property real labelWidth: label.implicitWidth
        property string label
        property string comboName
        property var options: []
        property int value: 0
        signal picked(int index)
        Layout.fillWidth: true
        Layout.preferredHeight: combo.implicitHeight
        spacing: 16
        Text { id: label; textFormat: Text.PlainText; Layout.fillWidth: true; text: picker.label; color: Theme.txt; font.pixelSize: 13; font.weight: Font.Medium; elide: Text.ElideRight }
        Item {
            Layout.preferredWidth: picker.controlWidth
            Layout.fillHeight: true
            NeoCombo { appWindow: picker.appWindow;
                id: combo
                objectName: picker.comboName
                anchors.fill: parent
                visible: picker.live
                enabled: controller.connected && controller.hasSpeakToChatConfig
                model: picker.options
                currentIndex: picker.live ? picker.value : -1
                Connections {
                    target: controller
                    function onStateChanged() { combo.currentIndex = Qt.binding(function() { return picker.live ? picker.value : -1 }) }
                }
                onActivated: picker.picked(index)
            }
            Placeholder {
                anchors.fill: parent
                visible: !picker.live
                loading: picker.loading
            }
        }
    }

    // Scrolls once a narrow window stacks the tiles.
    WheelScroll { flickable: flick }
    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.implicitHeight + 22 + appWindow.pageMargin
        boundsBehavior: Flickable.StopAtBounds
        // Wheel only: a drag is the slider's under the pointer.
        interactive: false
        clip: true
        ScrollBar.vertical: ScrollBar { policy: flick.contentHeight > flick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

        ColumnLayout {
            id: column
            x: appWindow.pageMargin
            y: 22
            width: flick.width - 2 * appWindow.pageMargin
            spacing: 16

            SectionTitle { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.fillHeight: false
                title: appWindow.tr("features_title")
                subtitle: appWindow.tr("features_desc")
            }

            // Headline feature: the upscaler gets the wide row.
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                // A fixed height, not one read back from the row: that loop
                // needed a third layout pass with Linux fonts. The narrow
                // window adds room for a wrapped description.
                Layout.preferredHeight: appWindow.compact ? 118 : 108
                objectName: "dseeCard"
                opacity: root.lacks(controller.hasDsee) ? 0.45 : 1
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: appWindow.compact ? 14 : 20
                    Rectangle {
                        visible: !appWindow.stacked
                        width: 72; height: 72; radius: 14
                        color: Theme.surfaceHi
                        border.width: 1
                        border.color: Theme.line
                        Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.sparkle; size: 32; weight: 1.5; color: Theme.txt }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Eyebrow { appWindow: root.appWindow; text: appWindow.tr("upscaling") }
                        Text { textFormat: Text.PlainText; text: "DSEE Extreme"; color: Theme.txt; font.pixelSize: 22; font.weight: Font.Bold; font.letterSpacing: -0.4 }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: !controller.hasDsee ? appWindow.tr("not_supported") : !root.known("dsee") ? appWindow.tr("state_unknown_waiting") : appWindow.tr("feat_dsee_desc")
                            color: Theme.txtDim
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.preferredWidth: 1
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                    Rectangle { visible: !appWindow.stacked; width: 1; Layout.fillHeight: true; color: Theme.line }
                    RowLayout {
                        spacing: 14
                        Text { textFormat: Text.PlainText; visible: !appWindow.stacked; text: controller.dsee ? appWindow.tr("active") : appWindow.tr("noise_control_off"); color: Theme.txt; font.pixelSize: 13; font.weight: Font.Medium }
                        NeoSwitch { appWindow: root.appWindow;
                            enabled: controller.hasDsee && controller.connected
                            confirmedChecked: controller.dsee
                            onToggled: controller.setDsee(checked)
                        }
                    }
                }
            }

            // Speak-to-Chat: the switch in the header, its settings below. Each
            // control shows what the headset has reported so far, in place of
            // a status line, and the card keeps one size throughout.
            Card { appWindow: root.appWindow;
                id: stcCard
                objectName: "speakToChatCard"
                // disconnected, unsupported, reading (on/off not known yet),
                // settingsPending (on/off known, settings not yet) or ready.
                readonly property string phase: !controller.connected ? "disconnected"
                    : !controller.hasSpeakToChat ? "unsupported"
                    : !root.known("speakToChat") ? "reading"
                    : controller.hasSpeakToChatConfig && !root.known("speakToChatConfig") ? "settingsPending"
                    : "ready"
                readonly property bool switchLive: phase === "settingsPending" || phase === "ready"
                // A model with the switch but not the settings keeps them greyed.
                readonly property bool settingsLocked: controller.connected && !controller.hasSpeakToChatConfig
                readonly property bool settingsLive: phase === "ready" && !settingsLocked
                readonly property bool settingsLoading: !settingsLocked && (phase === "reading" || phase === "settingsPending")
                // From the card's own width, not the window's.
                readonly property bool compact: width <= 380
                readonly property int headerHeight: compact ? 76 : 84
                // One width for both dropdowns, so they line up: 240, giving
                // way down to 150 before the longer label is cut short. From
                // the card's width, not the rows': a width computed from the
                // rows themselves makes the layout rearrange in a loop.
                readonly property real pickerWidth: Math.max(150, Math.min(240,
                    width - 36 - 16 - Math.ceil(Math.max(sensitivityRow.labelWidth, timeoutRow.labelWidth)) - 1))
                Layout.fillWidth: true
                // Three 40 px rows, 12 apart, inside the 18 px padding.
                Layout.preferredHeight: headerHeight + 1 + 18 + 3 * 40 + 2 * 12 + 18
                opacity: root.lacks(controller.hasSpeakToChat) ? 0.45 : 1
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: stcCard.headerHeight
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        spacing: stcCard.compact ? 12 : 16
                        Rectangle {
                            readonly property int size: stcCard.compact ? 40 : 48
                            width: size; height: size; radius: size / 2
                            color: Theme.surfaceHi
                            border.width: 1
                            border.color: Theme.line
                            Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.chat; size: stcCard.compact ? 20 : 22; weight: 1.6; color: stcCard.phase === "disconnected" ? Theme.txtFaint : Theme.txt }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: 2
                            Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: "Speak-to-Chat"; color: Theme.txt; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: root.lacks(controller.hasSpeakToChat) ? appWindow.tr("not_supported") : appWindow.tr("feat_speak_desc"); color: Theme.txtDim; font.pixelSize: 12; elide: Text.ElideRight }
                        }
                        Item {
                            implicitWidth: stcSwitch.implicitWidth
                            implicitHeight: stcSwitch.implicitHeight
                            NeoSwitch { appWindow: root.appWindow;
                                id: stcSwitch
                                objectName: "speakToChatSwitch"
                                anchors.fill: parent
                                visible: stcCard.switchLive
                                enabled: controller.connected && controller.hasSpeakToChat
                                confirmedChecked: controller.speakToChat
                                onToggled: controller.setSpeakToChat(checked)
                            }
                            Placeholder {
                                anchors.fill: parent
                                visible: !stcCard.switchLive
                                switchShape: true
                                loading: stcCard.phase === "reading"
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                    ColumnLayout {
                        objectName: "speakToChatSettings"
                        Layout.fillWidth: true
                        Layout.margins: 18
                        spacing: 12
                        // Greyed where the model lacks them; dimmed but still
                        // editable while Speak-to-Chat is off.
                        opacity: root.lacks(controller.hasSpeakToChat) ? 1 : stcCard.settingsLocked ? 0.45
                               : stcCard.phase === "ready" && !controller.speakToChat ? 0.55 : 1
                        StcPicker {
                            id: sensitivityRow
                            appWindow: root.appWindow
                            controlWidth: stcCard.pickerWidth
                            live: stcCard.settingsLive
                            loading: stcCard.settingsLoading
                            label: appWindow.tr("stc_sensitivity")
                            comboName: "stcSensitivityCombo"
                            // Wire codes, in order.
                            options: [appWindow.tr("stc_sensitivity_auto"), appWindow.tr("stc_sensitivity_high"), appWindow.tr("stc_sensitivity_low")]
                            value: controller.speakToChatSensitivity
                            onPicked: function(index) { controller.setSpeakToChatSensitivity(index) }
                        }
                        StcPicker {
                            id: timeoutRow
                            appWindow: root.appWindow
                            controlWidth: stcCard.pickerWidth
                            live: stcCard.settingsLive
                            loading: stcCard.settingsLoading
                            label: appWindow.tr("stc_timeout")
                            comboName: "stcTimeoutCombo"
                            options: [appWindow.tr("stc_timeout_short"), appWindow.tr("stc_timeout_standard"), appWindow.tr("stc_timeout_long"), appWindow.tr("stc_timeout_never")]
                            value: controller.speakToChatTimeout
                            onPicked: function(index) { controller.setSpeakToChatTimeout(index) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            spacing: 16
                            Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: appWindow.tr("stc_voice_passthrough"); color: Theme.txt; font.pixelSize: 13; font.weight: Font.Medium; elide: Text.ElideRight }
                            Item {
                                implicitWidth: passthroughSwitch.implicitWidth
                                implicitHeight: passthroughSwitch.implicitHeight
                                NeoSwitch { appWindow: root.appWindow;
                                    id: passthroughSwitch
                                    objectName: "stcVoicePassthroughSwitch"
                                    anchors.fill: parent
                                    visible: stcCard.settingsLive
                                    enabled: controller.connected && controller.hasSpeakToChatConfig
                                    confirmedChecked: controller.speakToChatVoicePassthrough
                                    onToggled: controller.setSpeakToChatVoicePassthrough(checked)
                                }
                                Placeholder {
                                    anchors.fill: parent
                                    visible: !stcCard.settingsLive
                                    switchShape: true
                                    loading: stcCard.settingsLoading
                                }
                            }
                        }
                    }
                }
            }

            Card { appWindow: root.appWindow;
                id: adaptiveTile
                readonly property bool ready: controller.hasAdaptiveVolume && root.known("adaptiveVolume")
                objectName: "adaptiveVolumeTile"
                opacity: root.lacks(controller.hasAdaptiveVolume) ? 0.45 : 1
                Layout.fillWidth: true
                Layout.preferredHeight: 168
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 8
                    Glyph { appWindow: root.appWindow; path: appWindow.icons.volume; size: 26; weight: 1.6; color: Theme.txt }
                    Text { textFormat: Text.PlainText; Layout.topMargin: 4; text: "Adaptive Volume"; color: Theme.txt; font.pixelSize: 15; font.weight: Font.DemiBold }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: !controller.hasAdaptiveVolume ? appWindow.tr("not_supported") : !adaptiveTile.ready ? appWindow.tr("state_unknown_waiting") : appWindow.tr("feat_adaptive_desc")
                        color: Theme.txtDim
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        Layout.preferredWidth: 1
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Item { Layout.fillHeight: true }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: controller.adaptiveVolume ? appWindow.tr("active") : appWindow.tr("noise_control_off"); color: Theme.txt; font.pixelSize: 12; font.weight: Font.Medium }
                        NeoSwitch { appWindow: root.appWindow;
                            enabled: controller.hasAdaptiveVolume && controller.connected
                            confirmedChecked: controller.adaptiveVolume
                            onToggled: controller.setAdaptiveVolume(checked)
                        }
                    }
                }
            }

            // Auto power off: the wide row with a picker on the right.
            Card { appWindow: root.appWindow;
                objectName: "autoPowerOffCard"
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                opacity: root.lacks(controller.hasAutoPowerOff) ? 0.45 : 1
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 16
                    Rectangle {
                        visible: !appWindow.compact
                        width: 48; height: 48; radius: 24
                        color: Theme.surfaceHi
                        border.width: 1
                        border.color: Theme.line
                        Glyph { appWindow: root.appWindow; anchors.centerIn: parent; path: appWindow.icons.clock; size: 22; weight: 1.6; color: Theme.txt }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { textFormat: Text.PlainText; text: appWindow.tr("auto_power_off"); color: Theme.txt; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Text { textFormat: Text.PlainText; Layout.fillWidth: true; text: root.lacks(controller.hasAutoPowerOff) ? appWindow.tr("not_supported") : appWindow.tr("auto_power_off_desc"); color: Theme.txtDim; font.pixelSize: 12; elide: Text.ElideRight }
                    }
                    // The dropdown once the headset has reported the setting;
                    // before that a placeholder of its size, as on Speak-to-Chat:
                    // dashed with no headset or no setting, pulsing while read.
                    Item {
                        id: powerSlot
                        readonly property bool live: root.known("autoPowerOff")
                        implicitWidth: appWindow.compact ? 132 : 150
                        implicitHeight: powerCombo.implicitHeight
                        NeoCombo { appWindow: root.appWindow;
                            id: powerCombo
                            objectName: "autoPowerOffCombo"
                            anchors.fill: parent
                            visible: powerSlot.live
                            enabled: controller.connected && controller.hasAutoPowerOff
                            model: appWindow.autoPowerOffChoices.map(function(choice) { return choice.label })
                            currentIndex: powerSlot.live ? appWindow.autoPowerOffChoice(controller.autoPowerOff) : -1
                            Connections {
                                target: controller
                                function onStateChanged() { powerCombo.currentIndex = Qt.binding(function() { return powerSlot.live ? appWindow.autoPowerOffChoice(controller.autoPowerOff) : -1 }) }
                            }
                            onActivated: controller.setAutoPowerOff(appWindow.autoPowerOffChoices[index].index)
                        }
                        Placeholder {
                            anchors.fill: parent
                            visible: !powerSlot.live
                            loading: controller.connected && controller.hasAutoPowerOff
                        }
                    }
                }
            }
        }
    }
}
