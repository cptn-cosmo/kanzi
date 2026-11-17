import QtQuick
import QtWebEngine
import QtWebChannel
import Kanzi

// Invisible host for the page that does the actual audio decoding: MusicKit JS for
// Apple Music (DRM), or plex.html for Plex.
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
        if (Player.canLoad())
            view.loadHtml(Player.playerHtml(), Player.playerBaseUrl())
        else
            view.loadHtml("<!doctype html><html></html>") // stop whatever the old page was playing
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
