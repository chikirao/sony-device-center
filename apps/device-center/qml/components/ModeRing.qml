import QtQuick
import ".."

// The halo around the product says the noise mode at a glance: a solid ring
// for noise cancelling (sealed), a ring of dots for ambient sound (open), a
// faint dust of finer dots with processing off. On a change the new ring
// draws itself in clockwise from the top while the old one fades.
//
// Repaints only while a transition runs or the size, theme or mode change.
Canvas {
    id: ring
    // "cancelling", "ambient", "off" or anything else for "not known".
    property string mode: "unknown"
    property color ink: Theme.txt
    readonly property real radius: Math.min(width, height) / 2 - 4

    property string shown: mode
    property string leaving: ""
    property real progress: 1

    onModeChanged: {
        if (mode === shown) return
        leaving = shown
        shown = mode
        // With motion off there is no transition to drive the repaint (a
        // zero-length animation leaves progress at 1, so nothing changes).
        if (Theme.motionEnabled) {
            progress = 0
            draw.restart()
        } else {
            draw.stop()
            leaving = ""
            progress = 1
            requestPaint()
        }
    }
    NumberAnimation {
        id: draw
        target: ring
        property: "progress"
        to: 1
        duration: Theme.duration(760)
        easing.type: Easing.OutCubic
        onFinished: ring.leaving = ""
    }

    onProgressChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onInkChanged: requestPaint()

    // One ring of `count` dots of radius `dot`, only those within `sweep`
    // (0..1 of the turn, from twelve o'clock).
    function dots(ctx, count, dot, sweep) {
        var cx = width / 2, cy = height / 2
        for (var i = 0; i < count; ++i) {
            if (i / count > sweep) break
            var a = -Math.PI / 2 + i * 2 * Math.PI / count
            ctx.beginPath()
            ctx.arc(cx + Math.cos(a) * radius, cy + Math.sin(a) * radius, dot, 0, 2 * Math.PI)
            ctx.fill()
        }
    }

    function paintMode(ctx, m, alpha, sweep) {
        if (alpha <= 0 || sweep <= 0) return
        ctx.fillStyle = ink
        ctx.strokeStyle = ink
        if (m === "cancelling") {
            ctx.globalAlpha = alpha
            ctx.lineWidth = 1.6
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, radius, -Math.PI / 2, -Math.PI / 2 + sweep * 2 * Math.PI, false)
            ctx.stroke()
        } else if (m === "ambient") {
            ctx.globalAlpha = alpha
            dots(ctx, 72, Math.max(1.3, radius * 0.0095), sweep)
        } else {
            // Off, and the waiting state: more dots, smaller, see-through.
            ctx.globalAlpha = alpha * (m === "off" ? 0.4 : 0.22)
            dots(ctx, 144, Math.max(0.8, radius * 0.0062), sweep)
        }
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        if (radius <= 0) return
        if (leaving !== "") paintMode(ctx, leaving, 1 - progress, 1)
        paintMode(ctx, shown, Math.min(1, 0.35 + progress), progress)
    }
}
