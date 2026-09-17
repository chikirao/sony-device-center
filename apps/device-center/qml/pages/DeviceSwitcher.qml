import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../components"

ViewPage {
    id: root
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        anchors.topMargin: 22
        spacing: 22

        SectionTitle { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: false
            eyebrow: appWindow.tr("easy_switch")
            title: appWindow.tr("paired_devices")
            subtitle: appWindow.tr("paired_devices_desc")
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
                    Behavior on scale { NumberAnimation { duration: Theme.tBase } }

                    HoverHandler { id: devHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: Theme.cardRadius
                            color: devCard.current ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : Theme.surfaceSunk
                            border.width: 1
                            border.color: devCard.current ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5) : Theme.line

                            Glyph { appWindow: root.appWindow;
                                anchors.centerIn: parent
                                path: appWindow.icons.headphones
                                size: 22
                                color: devCard.current ? Theme.accentSoft : Theme.txtFaint
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                textFormat: Text.PlainText
                                text: devCard.modelData.name
                                color: Theme.txt
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            RowLayout {
                                spacing: 7
                                Rectangle {
                                    Layout.preferredWidth: 6
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: devCard.current ? Theme.success : Theme.txtFaint
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: devCard.current ? appWindow.tr("connected") : appWindow.tr("available")
                                    color: devCard.current ? Theme.success : Theme.txtFaint
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: "·  " + devCard.modelData.address
                                    color: Theme.txtFaint
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
