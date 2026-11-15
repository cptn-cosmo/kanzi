pragma Singleton
import QtQuick

QtObject {
    // Follow the desktop colour scheme; dark unless the system explicitly asks for light.
    readonly property bool dark: Application.styleHints.colorScheme !== Qt.ColorScheme.Light

    readonly property color accent: "#ff4f6d"
    readonly property color accentHover: "#ff6b84"
    readonly property color accentText: "#ffffff"

    readonly property color bg: dark ? "#111113" : "#f7f7f8"
    readonly property color sidebar: dark ? "#17171a" : "#eeeef1"
    readonly property color bar: dark ? "#1a1a1e" : "#fbfbfc"
    readonly property color surface: dark ? "#1e1e22" : "#ffffff"
    readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.05)
    readonly property color pressed: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.09)
    readonly property color selected: dark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0, 0, 0, 0.07)
    readonly property color placeholder: dark ? "#26262b" : "#e2e2e6"
    readonly property color border: dark ? "#26262b" : "#dedee3"

    readonly property color text: dark ? "#f4f4f5" : "#18181b"
    readonly property color textDim: dark ? "#a1a1aa" : "#5f5f69"
    readonly property color textFaint: dark ? "#6b6b74" : "#94949e"

    readonly property int radiusSmall: 6
    readonly property int radius: 10
    readonly property int radiusLarge: 14

    readonly property int fontSmall: 12
    readonly property int fontBody: 14
    readonly property int fontLarge: 17
    readonly property int fontTitle: 28
    readonly property int fontHero: 40

    readonly property int gutter: 32

    function formatTime(seconds) {
        if (!isFinite(seconds) || seconds < 0)
            seconds = 0
        const s = Math.floor(seconds)
        const h = Math.floor(s / 3600)
        const m = Math.floor((s % 3600) / 60)
        const ss = String(s % 60).padStart(2, "0")
        return h > 0 ? `${h}:${String(m).padStart(2, "0")}:${ss}` : `${m}:${ss}`
    }

    function formatLongDuration(ms) {
        const minutes = Math.round(ms / 60000)
        if (minutes < 60)
            return minutes === 1 ? qsTr("1 minute") : qsTr("%1 minutes").arg(minutes)
        const h = Math.floor(minutes / 60)
        const m = minutes % 60
        return m > 0 ? qsTr("%1 hr %2 min").arg(h).arg(m) : qsTr("%1 hr").arg(h)
    }

    // Artwork payloads carry a dominant colour as bare hex.
    function artColor(artwork, fallback) {
        if (artwork && artwork.bgColor)
            return "#" + artwork.bgColor
        return fallback !== undefined ? fallback : placeholder
    }
}
