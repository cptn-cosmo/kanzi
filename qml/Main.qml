import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

ApplicationWindow {
    id: win

    width: 1320
    height: 860
    minimumWidth: 940
    minimumHeight: 620
    // Shown in Component.onCompleted unless starting in the tray.
    visible: false
    title: Player.hasTrack ? `${Player.nowPlaying.title} · ${Player.nowPlaying.artist} — Kanzi` : "Kanzi"
    color: Theme.bg

    palette {
        window: Theme.bg
        windowText: Theme.text
        base: Theme.surface
        alternateBase: Theme.sidebar
        text: Theme.text
        button: Theme.surface
        buttonText: Theme.text
        highlight: Theme.accent
        highlightedText: Theme.accentText
        placeholderText: Theme.textFaint
        mid: Theme.border
        dark: Theme.textDim
        light: Theme.surface
        toolTipBase: Theme.surface
        toolTipText: Theme.text
    }
    font.pixelSize: Theme.fontBody

    PlayerHost {
        id: host
    }

    // ---- window management: tray, mini player ----
    function showMain() {
        Nav.miniPlayerOpen = false
        win.show()
        win.raise()
        win.requestActivate()
    }

    function quit() {
        Qt.quit()
    }

    onClosing: close => {
        if (Tray.closeToTray) {
            close.accepted = false
            win.hide()
        } else {
            win.quit()
        }
    }

    MiniPlayer {
        id: mini
        visible: Nav.miniPlayerOpen
        onExpandRequested: win.showMain()
        onClosing: close => {
            Nav.miniPlayerOpen = false
            // Without a tray there would be no window left to get back to.
            if (!Tray.active)
                win.showMain()
        }
    }

    Connections {
        target: Nav
        function onMiniPlayerOpenChanged() {
            if (Nav.miniPlayerOpen) {
                win.hide()
                mini.raise()
                mini.requestActivate()
            }
        }
    }

    Connections {
        target: Tray
        function onToggleWindowRequested() {
            if (win.visible && win.active)
                win.hide()
            else
                win.showMain()
        }
        function onShowWindowRequested() { win.showMain() }
        function onMiniPlayerRequested() { Nav.miniPlayerOpen = true }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                Layout.fillHeight: true
                Layout.preferredWidth: 252
            }

            Rectangle {
                Layout.fillHeight: true
                implicitWidth: 1
                color: Theme.border
            }

            StackView {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                pushEnter: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "x"; from: 40; to: 0; duration: 260; easing.type: Easing.OutCubic }
                    }
                }
                pushExit: Transition {
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 }
                }
                popEnter: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
                }
                popExit: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 }
                        NumberAnimation { property: "x"; from: 0; to: 40; duration: 200; easing.type: Easing.InCubic }
                    }
                }
                replaceEnter: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                }
                replaceExit: Transition {
                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                }
            }
        }

        PlayerBar {
            Layout.fillWidth: true
        }
    }

    QueuePanel {
        id: queuePanel
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 88
        width: 360
        x: Nav.queueOpen && !Nav.nowPlayingOpen ? parent.width - width : parent.width
        Behavior on x { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    }

    NowPlaying {
        anchors.fill: parent
    }

    // ---- toast ----
    Rectangle {
        id: toast
        property string message
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height - 88 - height - 16
        width: toastText.implicitWidth + 32
        height: 38
        radius: height / 2
        color: Theme.dark ? "#2d2d33" : "#26262b"
        opacity: 0
        visible: opacity > 0
        z: 100

        Text {
            id: toastText
            anchors.centerIn: parent
            text: toast.message
            color: "#ffffff"
            font.pixelSize: Theme.fontBody
        }

        SequentialAnimation {
            id: toastAnim
            NumberAnimation { target: toast; property: "opacity"; to: 1; duration: 150 }
            PauseAnimation { duration: 2200 }
            NumberAnimation { target: toast; property: "opacity"; to: 0; duration: 300 }
        }
    }

    Connections {
        target: Nav
        function onToast(message) {
            toast.message = message
            toastAnim.restart()
        }
    }

    Connections {
        target: Player
        function onRaiseRequested() { win.showMain() }
        function onErrorStringChanged() {
            if (Player.errorString)
                Nav.toast(Player.errorString)
        }
    }

    property bool wasAuthorized: AppleMusic.authorized
    Connections {
        target: AppleMusic
        function onUserTokenChanged() {
            if (AppleMusic.authorized === win.wasAuthorized)
                return
            win.wasAuthorized = AppleMusic.authorized
            Nav.show(AppleMusic.authorized ? "home" : "welcome")
        }
    }

    // ---- keyboard ----
    readonly property bool typing: activeFocusItem instanceof TextInput

    Shortcut { sequence: "Space"; enabled: !win.typing; onActivated: Player.togglePlayPause() }
    Shortcut { sequence: "Ctrl+Right"; onActivated: Player.next() }
    Shortcut { sequence: "Ctrl+Left"; onActivated: Player.previous() }
    Shortcut { sequence: "Ctrl+Up"; onActivated: Player.volume = Math.min(1, Player.volume + 0.05) }
    Shortcut { sequence: "Ctrl+Down"; onActivated: Player.volume = Math.max(0, Player.volume - 0.05) }
    Shortcut { sequences: [StandardKey.Find, "Ctrl+L"]; onActivated: Nav.focusSearch() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: win.quit() }
    Shortcut { sequence: "Ctrl+M"; onActivated: Nav.miniPlayerOpen = true }
    Shortcut { sequences: [StandardKey.Back, "Alt+Left"]; onActivated: Nav.back() }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (Nav.nowPlayingOpen)
                Nav.nowPlayingOpen = false
            else if (Nav.queueOpen)
                Nav.queueOpen = false
            else
                Nav.back()
        }
    }

    Component.onCompleted: {
        if (!(Tray.active && Tray.startInTray))
            win.show()
        Nav.stack = stack
        Nav.show(AppleMusic.authorized ? "home" : "welcome")
    }
}
