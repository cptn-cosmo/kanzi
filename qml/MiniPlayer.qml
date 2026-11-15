import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window
import Kanzi

// Compact, frameless player. Drag anywhere to move, drag the corner to resize.
// Shows just the artwork until hovered, then reveals the controls.
Window {
    id: win

    signal expandRequested()

    width: 320
    height: 320
    minimumWidth: 220
    minimumHeight: 220
    maximumWidth: 720
    maximumHeight: 720
    // Fixed title: it's how the KWin keep-above script finds this window.
    title: "Kanzi Mini Player"
    color: "transparent"
    // Stay-on-top is honoured on X11 and by some Wayland compositors; harmless elsewhere.
    flags: Qt.Window | Qt.FramelessWindowHint | (Tray.miniPlayerOnTop ? Qt.WindowStaysOnTopHint : 0)

    readonly property var np: Player.nowPlaying

    // Wayland clients can't raise themselves above others, so ask KWin directly.
    // The short delay lets the compositor map the window first.
    function applyKeepAbove() {
        if (visible && Tray.kwin)
            Tray.setKeepAbove(title, Tray.miniPlayerOnTop)
    }
    Timer { id: keepAboveTimer; interval: 250; onTriggered: win.applyKeepAbove() }
    onVisibleChanged: if (visible) keepAboveTimer.restart()
    Connections {
        target: Tray
        function onSettingsChanged() { keepAboveTimer.restart() }
    }
    readonly property bool showControls: hover.hovered || seek.pressed || !Player.hasTrack

    Settings {
        id: settings
        category: "miniPlayer"
        property alias width: win.width
        property alias height: win.height
    }

    Shortcut { sequence: "Space"; onActivated: Player.togglePlayPause() }
    Shortcut { sequence: "Escape"; onActivated: win.expandRequested() }
    Shortcut { sequences: ["Ctrl+Right"]; onActivated: Player.next() }
    Shortcut { sequences: ["Ctrl+Left"]; onActivated: Player.previous() }

    Item {
        id: card
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: cardMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Rectangle {
            anchors.fill: parent
            color: win.np.artworkColor ? "#" + win.np.artworkColor : "#1a1a1e"
        }

        Artwork {
            anchors.fill: parent
            artwork: win.np.artwork || null
            pixelSize: Math.max(win.width, win.height)
            radius: 0
            fallbackIcon: "song"
        }

        // Legibility gradient, stronger while controls are visible.
        Rectangle {
            anchors.fill: parent
            opacity: win.showControls ? 1 : 0.7
            Behavior on opacity { NumberAnimation { duration: 200 } }
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, win.showControls ? 0.45 : 0) }
                GradientStop { position: 0.45; color: Qt.rgba(0, 0, 0, win.showControls ? 0.15 : 0) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.8) }
            }
        }

        HoverHandler { id: hover }
        DragHandler {
            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) win.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: win.expandRequested()
        }

        // ---- top bar ----
        RowLayout {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 6
            spacing: 0
            opacity: win.showControls ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 160 } }

            IconButton {
                iconName: "expand"
                iconSize: 16
                color: "white"
                tip: qsTr("Open Kanzi")
                onClicked: win.expandRequested()
            }
            IconButton {
                iconName: "pin"
                iconSize: 15
                color: "white"
                active: Tray.miniPlayerOnTop
                tip: qsTr("Keep on top")
                onClicked: Tray.miniPlayerOnTop = !Tray.miniPlayerOnTop
            }
            Item { Layout.fillWidth: true }
            IconButton {
                iconName: "queue"
                iconSize: 16
                color: "white"
                tip: qsTr("Up Next")
                onClicked: { win.expandRequested(); Nav.queueOpen = true }
            }
            IconButton {
                iconName: "close"
                iconSize: 16
                color: "white"
                tip: qsTr("Close")
                onClicked: win.close()
            }
        }

        // ---- bottom: metadata, seek and transport ----
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 14
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: Player.hasTrack ? (win.np.title || "") : qsTr("Nothing playing")
                color: "white"
                font.pixelSize: 15
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: Player.hasTrack
                text: win.np.artist || ""
                color: Qt.rgba(1, 1, 1, 0.75)
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                opacity: win.showControls ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 160 } }

                SeekBar {
                    id: seek
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    to: Math.max(1, Player.duration)
                    externalValue: Player.position
                    enabled: Player.hasTrack
                    trackColor: Qt.rgba(1, 1, 1, 0.25)
                    fillColor: "white"
                    onCommitted: value => Player.seek(value)
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: Theme.formatTime(seek.pressed ? seek.value : Player.position)
                        color: Qt.rgba(1, 1, 1, 0.65); font.pixelSize: 11
                        font.features: { "tnum": 1 }
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "-" + Theme.formatTime(Math.max(0, Player.duration - (seek.pressed ? seek.value : Player.position)))
                        color: Qt.rgba(1, 1, 1, 0.65); font.pixelSize: 11
                        font.features: { "tnum": 1 }
                    }
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6
                    IconButton {
                        iconName: "shuffle"; iconSize: 16; color: "white"
                        active: Player.shuffle
                        visible: win.width >= 280
                        onClicked: Player.shuffle = !Player.shuffle
                    }
                    IconButton {
                        iconName: "previous"; iconSize: 22; color: "white"
                        enabled: Player.hasTrack
                        onClicked: Player.previous()
                    }
                    IconButton {
                        iconName: Player.playing ? "pause" : "play"
                        iconSize: 30; color: "white"
                        enabled: Player.hasTrack
                        onClicked: Player.togglePlayPause()
                    }
                    IconButton {
                        iconName: "next"; iconSize: 22; color: "white"
                        enabled: Player.hasTrack && Player.canGoNext
                        onClicked: Player.next()
                    }
                    IconButton {
                        iconName: Player.repeatMode === Player.RepeatOne ? "repeatOne" : "repeat"
                        iconSize: 16; color: "white"
                        active: Player.repeatMode !== Player.RepeatNone
                        visible: win.width >= 280
                        onClicked: Player.cycleRepeat()
                    }
                }
            }
        }

        // ---- resize grip ----
        Item {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 20
            height: 20
            visible: win.showControls
            HoverHandler { cursorShape: Qt.SizeFDiagCursor }
            DragHandler {
                target: null
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                onActiveChanged: if (active) win.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
            }
        }
    }

    Item {
        id: cardMask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusLarge
            color: "black"
        }
    }
}
