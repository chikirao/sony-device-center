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
        anchors.margins: 36
        spacing: 22

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                spacing: 5
                Eyebrow { appWindow: root.appWindow; text: controller.connected ? appWindow.tr("connected_device") : appWindow.tr("offline") }
                Text {
                    textFormat: Text.PlainText
                    text: controller.deviceName
                    color: Theme.txt
                    font.pixelSize: 30
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.7
                }
            }

            Item { Layout.fillWidth: true }

            // Power off. Red, compact, and only offered while the
            // link is up: after it the headset simply disappears.
            PillButton { appWindow: root.appWindow;
                visible: controller.connected
                text: appWindow.tr("power_off")
                glyphPath: appWindow.icons.power
                tint: Theme.danger
                compact: true
                onClicked: controller.powerOff()
            }

            // Compact status chips
            Repeater {
                model: {
                    var pct = function(v) { return v >= 0 ? v + "%" : appWindow.tr("unknown") }
                    var chips = [{ k: appWindow.tr("codec"), v: controller.codec }]
                    if (controller.hasDualBattery) {
                        chips.push({ k: "L", v: pct(controller.batteryLeft) })
                        chips.push({ k: "R", v: pct(controller.batteryRight) })
                        if (controller.batteryCase >= 0) chips.push({ k: appWindow.tr("battery_case"), v: pct(controller.batteryCase) })
                    } else {
                        chips.push({ k: appWindow.tr("battery"), v: pct(controller.batteryLevel) })
                    }
                    chips.push({ k: appWindow.tr("mode"), v: controller.noiseControlMode === "unknown" ? appWindow.tr("unknown") : controller.noiseControlMode === "cancelling" ? appWindow.tr("mode_anc")
                                  : controller.noiseControlMode === "ambient" ? appWindow.tr("mode_ambient") : appWindow.tr("mode_off") })
                    return chips
                }

                delegate: Rectangle {
                    id: statChip
                    required property var modelData
                    implicitWidth: chipCol.implicitWidth + 30
                    implicitHeight: 54
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.line

                    ColumnLayout {
                        id: chipCol
                        anchors.centerIn: parent
                        spacing: 3
                        Eyebrow { appWindow: root.appWindow;
                            Layout.alignment: Qt.AlignHCenter
                            text: statChip.modelData.k
                        }
                        Text {
                            textFormat: Text.PlainText
                            Layout.alignment: Qt.AlignHCenter
                            text: statChip.modelData.v
                            color: Theme.txt
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }

        // Hero
        Card { appWindow: root.appWindow;
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 24
            clip: true

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 4

                // The aura belongs to the product, not the card.
                // Centering it here keeps the rings off the labels.
                Item {
                    id: heroStage
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 290
                    Layout.preferredHeight: 290

                    Bloom { appWindow: root.appWindow;
                        anchors.centerIn: parent
                        z: -1
                        width: 470; height: 470
                        tint: controller.noiseControlMode === "unknown" ? Theme.txtDim : controller.noiseControlMode === "cancelling" ? Theme.accent
                            : controller.noiseControlMode === "ambient" ? Theme.ambientWarm
                            : Theme.txtFaint
                        strength: controller.noiseControlMode === "off" ? 0.05 : 0.18
                        Behavior on strength { NumberAnimation { duration: Theme.tSlow } }
                    }

                    // Concentric rings. ANC pulls inward, Ambient opens outward.
                    Repeater {
                        model: 3
                        delegate: Rectangle {
                            id: auraRing
                            required property int index
                            readonly property bool inward: controller.noiseControlMode === "cancelling"
                            readonly property bool live: controller.noiseControlMode !== "off"

                            anchors.centerIn: parent
                            z: -1
                            width: 252 + index * 58
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: inward ? Theme.accent : Theme.ambientWarm
                            opacity: 0
                            visible: live

                            SequentialAnimation {
                                running: Theme.motionEnabled && !Theme.light && auraRing.live
                                loops: Animation.Infinite
                                PauseAnimation { duration: Theme.duration(auraRing.index * 700) }
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: auraRing; property: "opacity"
                                        from: 0.0; to: 0.28
                                        duration: Theme.duration(900); easing.type: Easing.OutQuad
                                    }
                                    NumberAnimation {
                                        target: auraRing; property: "scale"
                                        from: auraRing.inward ? 1.12 : 0.90
                                        to: 1.0
                                        duration: Theme.duration(900); easing.type: Easing.OutQuad
                                    }
                                }
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: auraRing; property: "opacity"
                                        to: 0.0
                                        duration: Theme.duration(1200); easing.type: Easing.InQuad
                                    }
                                    NumberAnimation {
                                        target: auraRing; property: "scale"
                                        to: auraRing.inward ? 0.88 : 1.14
                                        duration: Theme.duration(1200); easing.type: Easing.InQuad
                                    }
                                }
                                PauseAnimation { duration: Theme.duration(400) }
                            }
                        }
                    }

                    Image {
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/" + controller.heroImagePath
                        opacity: controller.connected ? 1.0 : 0.35
                        scale: heroHover.hovered ? 1.06 : 1.0

                        Behavior on scale { NumberAnimation { duration: Theme.duration(320); easing.type: Easing.OutCubic } }
                        Behavior on opacity { NumberAnimation { duration: Theme.tSlow } }

                        HoverHandler { id: heroHover }
                    }
                }

                Text {
                    textFormat: Text.PlainText
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 10
                    text: controller.noiseControlMode === "unknown" ? appWindow.tr("unknown") : controller.noiseControlMode === "cancelling" ? appWindow.tr("noise_cancelling")
                        : controller.noiseControlMode === "ambient" ? appWindow.tr("ambient_sound")
                        : appWindow.tr("nc_title_off")
                    color: Theme.txt
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.2
                }

                Text {
                    textFormat: Text.PlainText
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 2
                    text: controller.noiseControlMode === "unknown" ? appWindow.tr("unknown") : controller.noiseControlMode === "cancelling" ? appWindow.tr("nc_desc_cancelling")
                        : controller.noiseControlMode === "ambient" ? appWindow.tr("nc_desc_ambient").arg(controller.ambientLevel)
                        : appWindow.tr("nc_desc_off")
                    color: Theme.txtFaint
                    font.pixelSize: 12
                }

                // Quick actions
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 30
                    spacing: 11

                    PillButton { appWindow: root.appWindow;
                        text: appWindow.tr("noise_cancelling")
                        glyphPath: appWindow.icons.shield
                        tint: Theme.accent
                        active: controller.noiseControlMode === "cancelling"
                        onClicked: controller.setAnc(!active)
                    }

                    PillButton { appWindow: root.appWindow;
                        text: appWindow.tr("ambient")
                        glyphPath: appWindow.icons.mic
                        tint: Theme.ambientWarm
                        active: controller.noiseControlMode === "ambient"
                        onClicked: controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
                    }

                    PillButton { appWindow: root.appWindow;
                        text: appWindow.tr("noise_control_off")
                        glyphPath: appWindow.icons.power
                        tint: Theme.txtDim
                        active: controller.noiseControlMode === "off"
                        onClicked: controller.setNoiseControlOff()
                    }
                }

                // Secondary: a link to another screen, not a fourth mode.
                PillButton { appWindow: root.appWindow;
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 12
                    compact: true
                    text: appWindow.tr("eq_chip") + appWindow.trPreset(controller.equalizerPreset)
                    glyphPath: appWindow.icons.sliders
                    onClicked: appWindow.navIndex = 2
                }
            }
        }
    }
}
