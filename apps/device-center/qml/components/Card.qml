import QtQuick
import ".."
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Elevated card. Gradient fakes a top light source; hairline defines the edge.
Rectangle {
    required property var appWindow
    id: card
    property bool inverse: false
    property bool active: false
    property bool hovered: false
    radius: Theme.cardRadius
    border.width: 1
    border.color: active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                : hovered ? Theme.lineHi : Theme.line
    gradient: Gradient {
        GradientStop { position: 0.0; color: inverse ? Theme.sidebarSurface : card.hovered ? Theme.surfaceHi : Theme.surface }
        GradientStop { position: 1.0; color: inverse ? Theme.sidebarSurfaceSunk : Theme.light ? Theme.surface : Theme.surfaceSunk }
    }
    Behavior on border.color { ColorAnimation { duration: Theme.tBase } }
}
