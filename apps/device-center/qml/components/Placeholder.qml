import QtQuick
import QtQuick.Shapes
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

    // Qt Quick has no dashed border, so the outline is a path. A Shape rather
    // than a Canvas: the software renderer (Linux CI) crashed in drawPixmap
    // painting a Canvas that was being resized, and pages hold many of these.
    Shape {
        id: outline
        anchors.fill: parent
        visible: !ph.loading
        readonly property real lineWidth: ph.switchShape ? 1.5 : 1
        readonly property real inset: lineWidth / 2
        readonly property real r: Math.max(0, Math.min(ph.radius - inset, (width - 2 * inset) / 2, (height - 2 * inset) / 2))
        readonly property real x0: inset
        readonly property real y0: inset
        readonly property real x1: width - inset
        readonly property real y1: height - inset
        ShapePath {
            strokeColor: Theme.lineHi
            strokeWidth: outline.lineWidth
            fillColor: "transparent"
            strokeStyle: ShapePath.DashLine
            // In line widths: 4 px dashes, 3 px gaps.
            dashPattern: [4 / outline.lineWidth, 3 / outline.lineWidth]
            startX: outline.x0 + outline.r; startY: outline.y0
            PathLine { x: outline.x1 - outline.r; y: outline.y0 }
            PathArc { x: outline.x1; y: outline.y0 + outline.r; radiusX: outline.r; radiusY: outline.r }
            PathLine { x: outline.x1; y: outline.y1 - outline.r }
            PathArc { x: outline.x1 - outline.r; y: outline.y1; radiusX: outline.r; radiusY: outline.r }
            PathLine { x: outline.x0 + outline.r; y: outline.y1 }
            PathArc { x: outline.x0; y: outline.y1 - outline.r; radiusX: outline.r; radiusY: outline.r }
            PathLine { x: outline.x0; y: outline.y0 + outline.r }
            PathArc { x: outline.x0 + outline.r; y: outline.y0; radiusX: outline.r; radiusY: outline.r }
        }
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
