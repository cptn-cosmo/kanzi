import QtQuick
import QtWebEngine
import QtWebChannel
import Kanzi

// Invisible host for the MusicKit JS page that does the actual (DRM) audio decoding.
Item {
    id: root

    width: 1
    height: 1
    opacity: 0
    z: -1
    enabled: false

    readonly property var profile: AppleMusic.webProfile

    WebChannel {
        id: channel
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        profile: root.profile
        webChannel: channel
        settings.playbackRequiresUserGesture: false
        settings.javascriptCanOpenWindows: true
        settings.showScrollBars: false

        onNewWindowRequested: request => {
            login.show()
            login.raise()
            request.openIn(login.view)
        }
        onPermissionRequested: permission => permission.grant()
        onJavaScriptConsoleMessage: (level, message, line, source) => {
            if (level === WebEngineView.ErrorMessageLevel)
                console.warn("[musickit]", message, `(${source}:${line})`)
        }
        onRenderProcessTerminated: (status, code) => {
            console.warn("Kanzi: player process ended", status, code, "— restarting")
            restartTimer.start()
        }
    }

    Timer {
        id: restartTimer
        interval: 1500
        onTriggered: root.reload()
    }

    LoginWindow {
        id: login
    }

    function reload() {
        if (AppleMusic.developerToken)
            view.loadHtml(Player.playerHtml(), "https://music.apple.com/")
    }

    Connections {
        target: Player
        function onReloadRequested() { root.reload() }
    }

    Component.onCompleted: {
        channel.registerObjects({ bridge: Player })
        reload()
    }
}
