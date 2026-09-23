import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

// The product on a stage, ringed by its noise mode; the three modes to pick
// from; and two cards with what the header does not say: how long the
// charge lasts and how the sound is shaped. Wide windows put the controls
// in a column beside the stage, narrow ones under it.
ViewPage {
    id: root

    readonly property bool wide: width >= 900
    readonly property string mode: controller.connected ? controller.noiseControlMode : "unknown"

    readonly property var modes: [
        { mode: "cancelling", title: appWindow.tr("noise_cancelling"), glyph: appWindow.icons.shield, show: controller.hasAnc,
          detail: appWindow.tr("mode_short_cancelling"), longDetail: appWindow.tr("nc_desc_cancelling") },
        { mode: "ambient", title: appWindow.tr("ambient_sound"), glyph: appWindow.icons.ambient, show: controller.hasAmbient,
          detail: appWindow.tr("mode_short_ambient"), longDetail: appWindow.tr("nc_desc_ambient").arg(controller.ambientLevel) },
        { mode: "off", title: appWindow.tr("noise_control_off"), glyph: appWindow.icons.power, show: true,
          detail: appWindow.tr("nc_card_off"), longDetail: appWindow.tr("nc_desc_off") }
    ]
    function apply(mode) {
        if (mode === "cancelling") controller.setAnc(true)
        else if (mode === "ambient") controller.setAmbient(controller.ambientLevel, controller.focusOnVoice)
        else controller.setNoiseControlOff()
    }

    // The last day's charge, in 48 half-hour buckets, for the card's bars.
    // Reread when the log changes, not on a timer.
    property var levels: []
    function readLevels() {
        var now = Date.now(), span = 24 * 3600 * 1000, n = 48
        var samples = controller.batterySamples(now - span)
        var out = [], last = -1, j = 0
        for (var b = 0; b < n; ++b) {
            var edge = now - span + (b + 1) * span / n
            while (j < samples.length && samples[j].t <= edge) {
                last = samples[j].event === "disconnected" ? -1 : samples[j].level
                ++j
            }
            out.push(last)
        }
        levels = out
    }
    Component.onCompleted: readLevels()
    Connections { target: controller; function onBatteryHistoryChanged() { root.readLevels() } }

    GridLayout {
        anchors.fill: parent
        anchors.margins: appWindow.pageMargin
        anchors.topMargin: 12
        columns: root.wide ? 2 : 1
        columnSpacing: 32
        rowSpacing: 16

        // Stage
        Item {
            id: stage
            objectName: "overviewStage"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150

            readonly property real ringSize: Math.min(width, height) - 16
            ModeRing {
                id: ring
                objectName: "modeRing"
                anchors.centerIn: parent
                width: stage.ringSize
                height: width
                mode: root.mode
            }
            Image {
                id: product
                anchors.centerIn: parent
                width: stage.ringSize * 0.74
                height: width
                sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio), Math.ceil(height * Screen.devicePixelRatio))
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                source: "qrc:/" + controller.heroImagePath
                opacity: controller.connected ? 1.0 : 0.4
                scale: productHover.hovered ? 1.03 : 1.0
                Behavior on scale { NumberAnimation { duration: Theme.duration(320); easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: Theme.tSlow } }
                HoverHandler { id: productHover }
            }
        }

        // Controls
        ColumnLayout {
            Layout.fillWidth: !root.wide
            Layout.preferredWidth: root.wide ? 340 : -1
            Layout.maximumWidth: root.wide ? 380 : Number.POSITIVE_INFINITY
            Layout.alignment: Qt.AlignVCenter
            spacing: 12

            GridLayout {
                Layout.fillWidth: true
                columns: root.wide ? 1 : 3
                rowSpacing: 12
                columnSpacing: 12
                Repeater {
                    model: root.modes
                    delegate: ModeButton {
                        required property var modelData
                        objectName: "modeButton_" + modelData.mode
                        appWindow: root.appWindow
                        visible: modelData.show
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        tile: !root.wide
                        glyphPath: modelData.glyph
                        title: modelData.title
                        detail: root.wide ? modelData.longDetail : modelData.detail
                        current: root.mode === modelData.mode
                        enabled: controller.connected
                        onClicked: root.apply(modelData.mode)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.topMargin: root.wide ? 4 : 0
                spacing: 12

                // Time left
                LinkCard {
                    objectName: "timeLeftCard"
                    appWindow: root.appWindow
                    title: appWindow.tr("time_left")
                    onClicked: appWindow.navIndex = 5
                    DotText {
                        objectName: "timeLeftDots"
                        text: !controller.connected ? "—"
                            : controller.isCharging ? appWindow.tr("charging")
                            : controller.batteryMinutesLeft >= 0
                              ? Math.floor(controller.batteryMinutesLeft / 60) + ":" + ("0" + controller.batteryMinutesLeft % 60).slice(-2)
                              : "—"
                        dot: 3
                        maxWidth: parent ? parent.width : 120
                        color: Theme.txt
                    }
                    Item { Layout.fillHeight: true; Layout.minimumHeight: 8 }
                    // The last day, one bar per half hour.
                    Row {
                        id: bars
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        height: 24
                        spacing: 1
                        Repeater {
                            model: root.levels
                            Rectangle {
                                required property var modelData
                                width: Math.max(1, (bars.width - 47) / 48)
                                height: modelData < 0 ? 1 : Math.max(2, bars.height * modelData / 100)
                                anchors.bottom: parent.bottom
                                color: modelData < 0 ? Theme.line : Theme.txtFaint
                            }
                        }
                    }
                }

                // Equalizer
                LinkCard {
                    objectName: "equalizerCard"
                    appWindow: root.appWindow
                    title: appWindow.tr("nav_equalizer")
                    visible: controller.hasEqualizer
                    onClicked: appWindow.navIndex = 2
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: controller.connected && controller.equalizerPreset >= 0 ? appWindow.trPreset(controller.equalizerPreset) : "—"
                        color: Theme.txt
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Item { Layout.fillHeight: true; Layout.minimumHeight: 8 }
                    // The curve as five faders.
                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 10
                        height: 30
                        Repeater {
                            model: 5
                            Item {
                                required property int index
                                width: 8; height: 30
                                readonly property int v: controller.equalizerBands && controller.equalizerBands[index] !== undefined
                                                         ? controller.equalizerBands[index] : 0
                                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1; height: parent.height; color: Theme.line }
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 6; height: 10; radius: 2
                                    color: Theme.txt
                                    y: Math.max(0, Math.min(parent.height - height, parent.height / 2 - height / 2 - parent.v * 1.2))
                                    Behavior on y { NumberAnimation { duration: Theme.tBase; easing.type: Easing.OutCubic } }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // A small card that opens a page: title with a chevron, then content.
    component LinkCard: Card {
        id: linkCard
        property string title: ""
        signal clicked()
        default property alias content: linkColumn.data
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.preferredHeight: root.wide ? 136 : 118
        hovered: linkHover.hovered
        HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: linkCard.clicked() }
        Accessible.role: Accessible.Button
        Accessible.name: title
        ColumnLayout {
            id: linkColumn
            anchors.fill: parent
            anchors.margins: 16
            anchors.bottomMargin: 14
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: linkCard.title
                    color: Theme.txtDim
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                Glyph { appWindow: linkCard.appWindow; path: appWindow.icons.chevronRight; size: 14; color: Theme.txtFaint }
            }
        }
    }
}
