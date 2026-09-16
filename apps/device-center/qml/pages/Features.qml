import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../components"

ViewPage {
    id: root
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 22

        ColumnLayout {
            spacing: 5
            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("behaviour") }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("features_title")
                color: appWindow.txt
                font.pixelSize: 28
                font.weight: Font.DemiBold
                font.letterSpacing: -0.6
            }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("features_desc")
                color: appWindow.txtDim
                font.pixelSize: 13
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 14
            columnSpacing: 14

            Repeater {
                model: [
                    { key: "dsee",     title: "DSEE Extreme",    desc: appWindow.tr("feat_dsee_desc"),     glyph: appWindow.icons.sparkle },
                    { key: "speak",    title: "Speak-to-Chat",   desc: appWindow.tr("feat_speak_desc"),    glyph: appWindow.icons.mic },
                    { key: "adaptive", title: "Adaptive Volume", desc: appWindow.tr("feat_adaptive_desc"), glyph: appWindow.icons.sliders }
                ]

                delegate: Card { appWindow: root.appWindow;
                    id: featCard
                    required property var modelData
                    readonly property string featureKey: modelData.key === "speak" ? "speakToChat" : modelData.key === "adaptive" ? "adaptiveVolume" : "dsee"
                    readonly property var availability: controller.featureStatus[featureKey]
                    readonly property bool known: controller.connected && availability && availability.availability === "valid"
                    readonly property bool supported: modelData.key === "dsee" ? controller.hasDsee : modelData.key === "speak" ? controller.hasSpeakToChat : controller.hasAdaptiveVolume
                    enabled: supported && controller.connected
                    readonly property bool on: modelData.key === "dsee" ? controller.dsee
                                             : modelData.key === "speak" ? controller.speakToChat
                                             : controller.adaptiveVolume

                    Layout.fillWidth: true
                    Layout.preferredHeight: 112
                    hovered: featHover.hovered
                    active: on
                    HoverHandler { id: featHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 15

                        Rectangle {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            radius: 14
                            color: featCard.on ? Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.18) : appWindow.surfaceSunk
                            border.width: 1
                            border.color: featCard.on ? Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.5) : appWindow.line
                            Behavior on color { ColorAnimation { duration: appWindow.tBase } }
                            Behavior on border.color { ColorAnimation { duration: appWindow.tBase } }

                            Glyph { appWindow: root.appWindow;
                                anchors.centerIn: parent
                                path: featCard.modelData.glyph
                                size: 21
                                color: featCard.on ? appWindow.accentSoft : appWindow.txtFaint
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                textFormat: Text.PlainText
                                text: featCard.modelData.title
                                color: appWindow.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: !featCard.supported ? appWindow.tr("not_supported") : !featCard.known ? appWindow.tr("state_unknown_waiting") : featCard.modelData.desc
                                color: appWindow.txtFaint
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }

                        NeoSwitch { appWindow: root.appWindow;
                            confirmedChecked: featCard.on
                            onToggled: {
                                if (featCard.modelData.key === "dsee") controller.setDsee(checked)
                                else if (featCard.modelData.key === "speak") controller.setSpeakToChat(checked)
                                else controller.setAdaptiveVolume(checked)
                            }
                        }
                    }
                }
            }

            // Auto power-off — a choice, not a toggle
            Card { appWindow: root.appWindow;
                Layout.fillWidth: true
                Layout.preferredHeight: 112
                hovered: powerHover.hovered
                HoverHandler { id: powerHover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 15

                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        radius: 14
                        color: appWindow.surfaceSunk
                        border.width: 1
                        border.color: appWindow.line
                        Glyph { appWindow: root.appWindow;
                            anchors.centerIn: parent
                            path: appWindow.icons.power
                            size: 21
                            color: appWindow.txtFaint
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("auto_power_off")
                            color: appWindow.txt
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: appWindow.tr("auto_power_off_desc")
                            color: appWindow.txtFaint
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }

                    ComboBox {
                        id: powerCombo
                        implicitWidth: 134
                        implicitHeight: 38
                        model: [appWindow.tr("apo_off"), appWindow.tr("apo_5min"), appWindow.tr("apo_15min"), appWindow.tr("apo_30min"), appWindow.tr("apo_1h"), appWindow.tr("apo_3h")]
                        currentIndex: controller.featureStatus.autoPowerOff && controller.featureStatus.autoPowerOff.availability === "valid" ? controller.autoPowerOff : -1
                        Connections {
                            target: controller
                            function onStateChanged() { powerCombo.currentIndex = Qt.binding(function() {
                                return controller.featureStatus.autoPowerOff && controller.featureStatus.autoPowerOff.availability === "valid" ? controller.autoPowerOff : -1
                            }) }
                        }
                        onActivated: controller.setAutoPowerOff(index)

                        background: Rectangle {
                            radius: 11
                            color: powerCombo.hovered ? appWindow.surfaceHi : appWindow.surfaceSunk
                            border.width: 1
                            border.color: powerCombo.hovered ? appWindow.lineHi : appWindow.line
                            Behavior on color { ColorAnimation { duration: appWindow.tFast } }
                        }

                        contentItem: Text {
                            textFormat: Text.PlainText
                            leftPadding: 13
                            rightPadding: 28
                            text: powerCombo.displayText
                            color: appWindow.txt
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        indicator: Glyph { appWindow: root.appWindow;
                            x: powerCombo.width - width - 12
                            y: powerCombo.height / 2 - height / 2
                            size: 14
                            color: appWindow.txtFaint
                            path: appWindow.icons.chevron
                            rotation: powerCombo.popup.visible ? 180 : 0
                            Behavior on rotation { NumberAnimation { duration: appWindow.tBase } }
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
