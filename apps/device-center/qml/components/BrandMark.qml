import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// The mark alone. Sealed left earcup (noise cancelling), open
// right earcup (ambient) — the product in one glyph.
Item {
    required property var appWindow
    id: brandMark
    property real size: 24
    property color color: "#FFFFFF"
    // One SVG unit expressed in item pixels. ShapePath.scale only
    // scales geometry, so stroke widths have to be scaled by hand
    // or the mark thickens as it shrinks.
    readonly property real u: size / 24
    implicitWidth: size
    implicitHeight: size

    Shape {
        anchors.fill: parent
        antialiasing: Theme.iconAntialiasing
        // Geometry rendering needs multisampling to smooth ShapePath edges.
        layer.enabled: Theme.iconAntialiasing
        layer.samples: Theme.iconAntialiasing ? 4 : 0

        ShapePath {
            strokeColor: brandMark.color
            strokeWidth: 2 * brandMark.u
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            scale: Qt.size(brandMark.u, brandMark.u)
            PathSvg { path: "M4.6 13.2 A7.2 7.2 0 0 1 19 13.2" }
        }

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: -1
            fillColor: brandMark.color
            scale: Qt.size(brandMark.u, brandMark.u)
            PathAngleArc {
                centerX: 4.6; centerY: 16
                radiusX: 2.6; radiusY: 2.6
                startAngle: 0; sweepAngle: 360
            }
        }

        ShapePath {
            strokeColor: brandMark.color
            strokeWidth: 1.7 * brandMark.u
            fillColor: "transparent"
            scale: Qt.size(brandMark.u, brandMark.u)
            PathAngleArc {
                centerX: 19; centerY: 16
                radiusX: 2.2; radiusY: 2.2
                startAngle: 0; sweepAngle: 360
            }
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.tFast } }
}
