pragma Singleton
import QtQuick

QtObject {
    readonly property color sidebarBg: "#0A0B0F"
    readonly property color sidebarSurface: "#14161E"
    readonly property color sidebarSurfaceHi: "#1B1E29"
    readonly property color sidebarSurfaceSunk: "#0E1016"
    readonly property color sidebarLine: "#22252F"
    readonly property color sidebarLineHi: "#2F3341"
    readonly property color sidebarAccent: "#7C5CFF"
    readonly property color sidebarAccentSoft: "#A78BFA"
    readonly property color sidebarAmbientWarm: "#F2A73B"
    readonly property color sidebarSuccess: "#2DD4A7"
    readonly property color sidebarDanger: "#FF5A5F"
    readonly property color sidebarTxt: "#F4F6FA"
    readonly property color sidebarTxtDim: "#98A1B2"
    readonly property color sidebarTxtFaint: "#5C6473"
    property bool iconAntialiasing: true
    property string mode: "dark"
    property bool animationsEnabled: true
    property bool systemReducedMotion: false
    readonly property bool light: mode === "light" || (mode === "system" && Qt.styleHints.colorScheme === Qt.Light)
    readonly property bool motionEnabled: animationsEnabled && !systemReducedMotion
    readonly property real cardRadius: light ? 12 : 18
    readonly property real controlRadius: 12
    readonly property string bodyFamily: Qt.application.font.family
    function duration(milliseconds) { return motionEnabled ? milliseconds : 0 }
    readonly property color bg:            light ? "#F5F5F5" : "#0A0B0F"
    readonly property color surface:       light ? "#FAFAFA" : "#14161E"
    readonly property color surfaceHi:     light ? "#EEEEEF" : "#1B1E29"
    readonly property color surfaceSunk:   light ? "#EFEFF0" : "#0E1016"
    readonly property color line:          light ? "#DDDEE1" : "#22252F"
    readonly property color lineHi:        light ? "#BCBEC3" : "#2F3341"

    readonly property color accent:        light ? "#151515" : "#7C5CFF"
    readonly property color accentSoft:    light ? "#414141" : "#A78BFA"
    readonly property color ambientWarm:   light ? "#6C532A" : "#F2A73B"
    readonly property color success:       light ? "#23634E" : "#2DD4A7"
    readonly property color danger:        light ? "#B12C36" : "#FF5A5F"

    readonly property color txt:           light ? "#111111" : "#F4F6FA"
    readonly property color txtDim:        light ? "#64666C" : "#98A1B2"
    readonly property color txtFaint:      light ? "#75777D" : "#5C6473"

    // Mono face for the wordmark. Resolved against the installed families
    // rather than set through font.families: that property makes the engine
    // abort loading the component on Qt 6.11, and silently — no error text.
    readonly property string monoFamily: {
        var wanted = ["JetBrains Mono", "JetBrainsMono Nerd Font", "Cascadia Code",
                      "SF Mono", "Consolas", "DejaVu Sans Mono", "Liberation Mono"]
        var installed = Qt.fontFamilies()
        for (var i = 0; i < wanted.length; ++i)
            if (installed.indexOf(wanted[i]) !== -1)
                return wanted[i]
        return "monospace"
    }

    // Motion constants — one place to retune the whole app's feel.
    readonly property int   tFast:  motionEnabled ? 140 : 0
    readonly property int   tBase:  motionEnabled ? 200 : 0
    readonly property int   tSlow:  motionEnabled ? 340 : 0

}
