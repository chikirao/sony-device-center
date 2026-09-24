import QtQuick
import ".."

// A QR code in the app's dot style: round dots, rounded finder squares, on
// a white card whatever the theme (scanners want dark on light). Painted
// once per size; the matrix never changes while it is shown.
Rectangle {
    id: root
    // Rows of '#' (dark) and '.', no quiet zone.
    property var matrix: []
    readonly property int modules: matrix.length
    readonly property int quiet: 3
    readonly property real cell: modules > 0 ? width / (modules + 2 * quiet) : 0

    implicitWidth: 200
    implicitHeight: width
    radius: cell * 2
    color: "#FFFFFF"

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var n = root.modules, m = root.cell, q = root.quiet
            if (n === 0 || m <= 0) return
            ctx.fillStyle = "#111111"
            function rounded(x, y, w, r) {
                ctx.moveTo(x + r, y)
                ctx.arcTo(x + w, y, x + w, y + w, r)
                ctx.arcTo(x + w, y + w, x, y + w, r)
                ctx.arcTo(x, y + w, x, y, r)
                ctx.arcTo(x, y, x + w, y, r)
                ctx.closePath()
            }
            var finders = [[0, 0], [0, n - 7], [n - 7, 0]]
            for (var f = 0; f < 3; ++f) {
                var x = (finders[f][1] + q) * m, y = (finders[f][0] + q) * m
                ctx.beginPath()
                rounded(x, y, 7 * m, 2 * m)
                ctx.fill()
                ctx.fillStyle = "#FFFFFF"
                ctx.beginPath()
                rounded(x + m, y + m, 5 * m, m)
                ctx.fill()
                ctx.fillStyle = "#111111"
                ctx.beginPath()
                rounded(x + 2 * m, y + 2 * m, 3 * m, m)
                ctx.fill()
            }
            ctx.beginPath()
            for (var r = 0; r < n; ++r) {
                var row = root.matrix[r]
                for (var c = 0; c < n; ++c) {
                    if (row.charAt(c) !== "#") continue
                    if ((r < 7 && (c < 7 || c >= n - 7)) || (r >= n - 7 && c < 7)) continue
                    var cx = (c + q + 0.5) * m, cy = (r + q + 0.5) * m
                    ctx.moveTo(cx + m * 0.46, cy)
                    ctx.arc(cx, cy, m * 0.46, 0, 2 * Math.PI)
                }
            }
            ctx.fill()
        }
    }
    onWidthChanged: canvas.requestPaint()
    onMatrixChanged: canvas.requestPaint()
}
