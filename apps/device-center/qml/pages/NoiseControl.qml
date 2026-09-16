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
        spacing: 24

        ColumnLayout {
            spacing: 5
            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("isolation") }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("nav_noise_control")
                color: appWindow.txt
                font.pixelSize: 28
                font.weight: Font.DemiBold
                font.letterSpacing: -0.6
            }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("nc_page_desc")
                color: appWindow.txtDim
                font.pixelSize: 13
            }
        }

        // Mode cards — big targets, honest states
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            spacing: 14

            Repeater {
                model: [
                    { mode: "cancelling", label: appWindow.tr("noise_cancelling"),  glyph: appWindow.icons.shield, desc: appWindow.tr("nc_card_cancelling"), tint: appWindow.accent },
                    { mode: "ambient",    label: appWindow.tr("ambient_sound"),     glyph: appWindow.icons.mic,    desc: appWindow.tr("nc_card_ambient"),    tint: appWindow.ambientWarm },
                    { mode: "off",        label: appWindow.tr("noise_control_off"), glyph: appWindow.icons.power,  desc: appWindow.tr("nc_card_off"),        tint: appWindow.txtDim }
                ]

                delegate: Card { appWindow: root.appWindow;
                    id: modeCard
                    required property var modelData
                    readonly property bool current: controller.noiseControlMode === modelData.mode

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: current
                    hovered: modeHover.hovered
                    scale: modeTap.pressed ? 0.975 : (modeHover.hovered ? 1.012 : 1.0)

                    Behavior on scale {
                        NumberAnimation { duration: 220; easing.type: Easing.OutBack; easing.overshoot: 1.8 }
                    }

                    HoverHandler { id: modeHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        id: modeTap
                        onTapped: {
                            if (modeCard.modelData.mode === "cancelling") controller.setAnc(true)
                            else if (modeCard.modelData.mode === "ambient") controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
                            else controller.setNoiseControlOff()
                        }
                    }

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: 13
                                color: modeCard.current
                                     ? Qt.rgba(modeCard.modelData.tint.r, modeCard.modelData.tint.g, modeCard.modelData.tint.b, 0.18)
                                     : appWindow.surfaceSunk
                                border.width: 1
                                border.color: modeCard.current
                                     ? Qt.rgba(modeCard.modelData.tint.r, modeCard.modelData.tint.g, modeCard.modelData.tint.b, 0.5)
                                     : appWindow.line
                                Behavior on color { ColorAnimation { duration: appWindow.tBase } }
                                Behavior on border.color { ColorAnimation { duration: appWindow.tBase } }

                                Glyph { appWindow: root.appWindow;
                                    anchors.centerIn: parent
                                    path: modeCard.modelData.glyph
                                    size: 21
                                    color: modeCard.current ? modeCard.modelData.tint : appWindow.txtFaint
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // Active dot, animated in
                            Rectangle {
                                Layout.preferredWidth: 9
                                Layout.preferredHeight: 9
                                radius: 4.5
                                color: modeCard.modelData.tint
                                opacity: modeCard.current ? 1 : 0
                                scale: modeCard.current ? 1 : 0.4
                                Behavior on opacity { NumberAnimation { duration: appWindow.tBase } }
                                Behavior on scale { NumberAnimation { duration: 280; easing.type: Easing.OutBack; easing.overshoot: 3 } }
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Text {
                                textFormat: Text.PlainText
                                text: modeCard.modelData.label
                                color: appWindow.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: modeCard.modelData.desc
                                color: appWindow.txtFaint
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }

        // Ambient detail
        Card { appWindow: root.appWindow;
            id: ambientCard
            Layout.fillWidth: true
            Layout.preferredHeight: 178
            readonly property bool live: controller.noiseControlMode === "ambient"
            opacity: live ? 1.0 : 0.42
            enabled: live
            Behavior on opacity { NumberAnimation { duration: appWindow.tSlow; easing.type: Easing.OutQuad } }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 18

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 2
                        Eyebrow { appWindow: root.appWindow; text: appWindow.tr("ambient") }
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("sound_level")
                            color: appWindow.txt
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: 56
                        implicitHeight: 34
                        radius: 11
                        color: Qt.rgba(appWindow.ambientWarm.r, appWindow.ambientWarm.g, appWindow.ambientWarm.b, 0.16)
                        border.width: 1
                        border.color: Qt.rgba(appWindow.ambientWarm.r, appWindow.ambientWarm.g, appWindow.ambientWarm.b, 0.5)
                        Text {
                            textFormat: Text.PlainText
                            anchors.centerIn: parent
                            text: Math.round(ambientSlider.value)
                            color: appWindow.ambientWarm
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                    }
                }

                NeoSlider { appWindow: root.appWindow;
                    id: ambientSlider
                    Layout.fillWidth: true
                    from: 1; to: 20; stepSize: 1
                    confirmedValue: controller.ambientLevel
                    onMoved: controller.setAmbient(Math.round(value), voiceSwitch.checked)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("focus_on_voice")
                            color: appWindow.txt
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: appWindow.tr("focus_on_voice_desc")
                            color: appWindow.txtFaint
                            font.pixelSize: 11
                        }
                    }
                    NeoSwitch { appWindow: root.appWindow;
                        id: voiceSwitch
                        confirmedChecked: controller.focusOnVoice
                        onToggled: controller.setAmbient(controller.ambientLevel, checked)
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
