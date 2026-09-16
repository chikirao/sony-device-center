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
            Eyebrow { appWindow: root.appWindow; text: appWindow.tr("easy_switch") }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("paired_devices")
                color: appWindow.txt
                font.pixelSize: 28
                font.weight: Font.DemiBold
                font.letterSpacing: -0.6
            }
            Text {
                textFormat: Text.PlainText
                text: appWindow.tr("paired_devices_desc")
                color: appWindow.txtDim
                font.pixelSize: 13
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 11

            Repeater {
                model: controller.pairedDevices

                delegate: Card { appWindow: root.appWindow;
                    id: devCard
                    required property var modelData
                    readonly property bool current: modelData.name === controller.deviceName

                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    active: current
                    hovered: devHover.hovered
                    scale: (devHover.hovered && !current) ? 1.006 : 1.0
                    Behavior on scale { NumberAnimation { duration: appWindow.tBase } }

                    HoverHandler { id: devHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: 15
                            color: devCard.current ? Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.18) : appWindow.surfaceSunk
                            border.width: 1
                            border.color: devCard.current ? Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.5) : appWindow.line

                            Glyph { appWindow: root.appWindow;
                                anchors.centerIn: parent
                                path: appWindow.icons.headphones
                                size: 22
                                color: devCard.current ? appWindow.accentSoft : appWindow.txtFaint
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                textFormat: Text.PlainText
                                text: devCard.modelData.name
                                color: appWindow.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            RowLayout {
                                spacing: 7
                                Rectangle {
                                    Layout.preferredWidth: 6
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: devCard.current ? appWindow.success : appWindow.txtFaint
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: devCard.current ? appWindow.tr("connected") : appWindow.tr("available")
                                    color: devCard.current ? appWindow.success : appWindow.txtFaint
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: "·  " + devCard.modelData.address
                                    color: appWindow.txtFaint
                                    font.pixelSize: 11
                                }
                            }
                        }

                        PillButton { appWindow: root.appWindow;
                            // Fixed width so the action column lines
                            // up down the list regardless of label.
                            Layout.preferredWidth: 132
                            Layout.alignment: Qt.AlignVCenter
                            implicitHeight: 40
                            text: devCard.current ? appWindow.tr("active") : appWindow.tr("connect")
                            glyphPath: devCard.current ? "" : appWindow.icons.swap
                            active: devCard.current
                            enabled: !devCard.current
                            opacity: enabled ? 1.0 : 0.8
                            onClicked: controller.connectDevice(devCard.modelData.address, devCard.modelData.name)
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
