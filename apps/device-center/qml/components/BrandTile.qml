import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// The mark in its gradient tile. Corner radius and mark inset are
// the ratios from assets/lockup.svg (r13 and 26.67 on a 40px tile).
Rectangle {
    required property var appWindow
    id: brandTile
    radius: width * 0.325
    gradient: Gradient {
        GradientStop { position: 0.0; color: appWindow.accentSoft }
        GradientStop { position: 1.0; color: appWindow.accent }
    }

    BrandMark { appWindow: brandTile.appWindow;
        anchors.centerIn: parent
        size: brandTile.width * 0.6667
        color: "#FFFFFF"
    }

    // Top sheen, matching the app icon's highlight pass.
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0.0;  color: Qt.rgba(1, 1, 1, 0.16) }
            GradientStop { position: 0.55; color: Qt.rgba(1, 1, 1, 0.0) }
        }
    }
}
