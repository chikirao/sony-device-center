import QtQuick
import ".."

// Stands in for a switch or a picker whose value is not known, at the
// control's own size so nothing moves when the value arrives. Unknown (no
// headset to ask): a dashed outline. Loading (asking it): a pulsing block,
// or for a switch a pulsing dash in a solid pill.
Item {
    id: ph
    property bool loading: false
    // A switch's pill with a short dash, instead of a picker's field.
    property bool switchShape: false
    readonly property real radius: switchShape ? height / 2 : Theme.controlRadius

    // 0.15 to 1 and back; held at 1 when motion is off.
    property real pulse: 1
    SequentialAnimation on pulse {
        running: ph.loading && ph.visible && Theme.motionEnabled
        loops: Animation.Infinite
        NumberAnimation { from: 0.15; to: 1; duration: 550; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0.15; duration: 550; easing.type: Easing.InOutSine }
        onRunningChanged: if (!running) ph.pulse = 1
    }

    // Qt Quick has no dashed border, so the outline is drawn.
    Canvas {
        id: outline
        anchors.fill: parent
        visible: !ph.loading
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Theme.lineHi
            ctx.lineWidth = ph.switchShape ? 1.5 : 1
            // Older Qt draws it solid.
            if (ctx.setLineDash) ctx.setLineDash([4, 3])
            var inset = ctx.lineWidth / 2
            ctx.beginPath()
            ctx.roundedRect(inset, inset, width - 2 * inset, height - 2 * inset, ph.radius - inset, ph.radius - inset)
            ctx.stroke()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: Theme; function onLightChanged() { outline.requestPaint() } }
    }

    // Loading.
    Rectangle {
        anchors.fill: parent
        visible: ph.loading
        radius: ph.radius
        color: ph.switchShape ? Theme.surfaceHi : Theme.lineHi
        border.width: ph.switchShape ? 1.5 : 0
        border.color: Theme.lineHi
        opacity: ph.switchShape ? 1 : ph.pulse
    }

    // The switch's dash: faint when unknown, bright and pulsing while loading.
    Rectangle {
        visible: ph.switchShape
        anchors.centerIn: parent
        width: ph.loading ? 17 : 13
        height: 3
        radius: 1.5
        color: ph.loading ? Theme.txt : Theme.txtFaint
        opacity: ph.loading ? ph.pulse : 1
    }
}
