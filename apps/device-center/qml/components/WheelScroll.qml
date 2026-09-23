import QtQuick

// Wheel and touchpad scrolling for a Flickable whose drag is switched off:
// on a desktop a press-and-drag belongs to the slider under it, not to the
// page. Put it in the Flickable's parent, over the same area.
WheelHandler {
    id: handler
    required property Flickable flickable
    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    onWheel: function(event) {
        var f = handler.flickable
        var max = Math.max(0, f.contentHeight - f.height)
        if (max <= 0) return
        // Touchpads report pixels; wheels report 120 per notch.
        var dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 120 * 72
        f.contentY = Math.max(0, Math.min(max, f.contentY - dy))
    }
}
