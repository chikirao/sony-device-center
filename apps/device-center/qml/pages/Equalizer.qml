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
        spacing: 20

        ColumnLayout {
            spacing: 5
            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("signature") }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("nav_equalizer")
                color: appWindow.txt
                font.pixelSize: 28
                font.weight: Font.DemiBold
                font.letterSpacing: -0.6
            }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("eq_page_desc")
                color: appWindow.txtDim
                font.pixelSize: 13
            }
        }

        // Presets — wrapping flow, not ten crushed columns
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { id: 0x00, name: appWindow.trPreset(0x00) },
                    { id: 0x16, name: appWindow.trPreset(0x16) },
                    { id: 0x15, name: appWindow.trPreset(0x15) },
                    { id: 0x14, name: appWindow.trPreset(0x14) },
                    { id: 0x10, name: appWindow.trPreset(0x10) },
                    { id: 0x11, name: appWindow.trPreset(0x11) },
                    { id: 0x12, name: appWindow.trPreset(0x12) },
                    { id: 0x13, name: appWindow.trPreset(0x13) },
                    { id: 0x17, name: appWindow.trPreset(0x17) },
                    { id: 0xa0, name: appWindow.trPreset(0xa0) }
                ]

                delegate: Rectangle {
                    id: chip
                    required property var modelData
                    readonly property bool current: controller.equalizerPreset === modelData.id

                    width: chipText.implicitWidth + 30
                    height: 38
                    radius: 19
                    color: current ? Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.18)
                         : chipHover.hovered ? appWindow.surfaceHi : appWindow.surface
                    border.width: 1
                    border.color: current ? Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.7)
                                : chipHover.hovered ? appWindow.lineHi : appWindow.line

                    Behavior on color { ColorAnimation { duration: appWindow.tFast } }
                    Behavior on border.color { ColorAnimation { duration: appWindow.tFast } }

                    scale: chipTap.pressed ? 0.94 : (chipHover.hovered ? 1.04 : 1.0)
                    Behavior on scale {
                        NumberAnimation { duration: 200; easing.type: Easing.OutBack; easing.overshoot: 2.4 }
                    }

                    HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { id: chipTap; onTapped: controller.setEqualizerPreset(chip.modelData.id) }

                    Text {
                        textFormat: Text.PlainText
                        id: chipText
                        anchors.centerIn: parent
                        text: chip.modelData.name
                        color: chip.current ? appWindow.txt : appWindow.txtDim
                        font.pixelSize: 12
                        font.weight: chip.current ? Font.DemiBold : Font.Normal
                        Behavior on color { ColorAnimation { duration: appWindow.tFast } }
                    }
                }
            }
        }

        // The five bands — the reason anyone opens this screen
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    Eyebrow { appWindow: root.appWindow; text: "5-Band · ±10 dB" }
                    Item { Layout.fillWidth: true }
                    Text {
                        textFormat: Text.PlainText
                        text: controller.equalizerPresetName
                        color: appWindow.accentSoft
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4

                    Repeater {
                        model: ["400", "1k", "2.5k", "6.3k", "16k"]

                        delegate: BandSlider { appWindow: root.appWindow;
                            id: bandItem
                            required property int index
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            label: modelData
                            value: (controller.equalizerBands && controller.equalizerBands[index] !== undefined)
                                   ? controller.equalizerBands[index] : 0

                            onMoved: function(v) {
                                var next = []
                                for (var i = 0; i < 5; ++i) {
                                    next.push(i === bandItem.index
                                        ? Math.round(v)
                                        : ((controller.equalizerBands && controller.equalizerBands[i] !== undefined)
                                            ? controller.equalizerBands[i] : 0))
                                }
                                controller.setEqualizerCustom(controller.clearBass, next)
                            }
                        }
                    }
                }
            }
        }

        // Clear Bass
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.preferredHeight: 96

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 22

                ColumnLayout {
                    Layout.preferredWidth: 210
                    spacing: 2
                    Text {
                        textFormat: Text.PlainText
                        text: appWindow.tr("clear_bass")
                        color: appWindow.txt
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: appWindow.tr("clear_bass_desc")
                        color: appWindow.txtFaint
                        font.pixelSize: 11
                    }
                }

                NeoSlider { appWindow: root.appWindow;
                    Layout.fillWidth: true
                    from: -10; to: 10; stepSize: 1
                    confirmedValue: controller.clearBass
                    onMoved: controller.setEqualizerCustom(Math.round(value), controller.equalizerBands)
                }

                Rectangle {
                    implicitWidth: 56
                    implicitHeight: 34
                    radius: 11
                    color: Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.16)
                    border.width: 1
                    border.color: Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.5)
                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: (controller.clearBass > 0 ? "+" : "") + controller.clearBass
                        color: appWindow.accentSoft
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }
}
