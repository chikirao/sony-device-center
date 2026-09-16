import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Elevated card. Gradient fakes a top light source; hairline defines the edge.
Rectangle {
    required property var appWindow
    id: card
    property bool active: false
    property bool hovered: false
    radius: 18
    border.width: 1
    border.color: active ? Qt.rgba(appWindow.accent.r, appWindow.accent.g, appWindow.accent.b, 0.55)
                : hovered ? appWindow.lineHi : appWindow.line
    gradient: Gradient {
        GradientStop { position: 0.0; color: card.hovered ? appWindow.surfaceHi : appWindow.surface }
        GradientStop { position: 1.0; color: appWindow.surfaceSunk }
    }
    Behavior on border.color { ColorAnimation { duration: appWindow.tBase } }
}
