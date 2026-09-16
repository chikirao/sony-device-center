import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// Every view enters the same way. Consistency is the whole point.
Item {
    required property var appWindow
    id: page
    default property alias pageData: inner.data

    Item {
        id: inner
        width: parent.width
        height: parent.height
        opacity: 0
    }

    ParallelAnimation {
        id: entrance
        NumberAnimation { target: inner; property: "opacity"; from: 0; to: 1; duration: appWindow.tBase + 60; easing.type: Easing.OutQuad }
        NumberAnimation { target: inner; property: "y"; from: 16; to: 0; duration: appWindow.tSlow; easing.type: Easing.OutCubic }
    }

    onVisibleChanged: if (visible) entrance.restart()
    Component.onCompleted: if (visible) entrance.restart()
}
