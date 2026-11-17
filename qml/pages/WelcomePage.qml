import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Item {
    id: root

    // ---- Plex ----
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 64)
        spacing: 18
        visible: Nav.plex

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/qt/qml/Kanzi/resources/kanzi.svg"
            sourceSize: Qt.size(112, 112)
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Connect to Plex")
            color: Theme.text
            font.pixelSize: Theme.fontHero - 6
            font.weight: Font.Bold
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Plex.signingIn
                  ? qsTr("Finish signing in in your browser. Kanzi continues on its own once you're done.")
                  : qsTr("Sign in with your Plex account to play the music libraries on your Plex Media Server.")
            color: Theme.textDim
            font.pixelSize: Theme.fontBody + 1
            lineHeight: 1.25
        }
        PillButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            text: Plex.signingIn ? qsTr("Waiting for Plex…") : qsTr("Sign in with Plex")
            iconName: "artist"
            enabled: !Plex.signingIn
            onClicked: Plex.signIn()
        }
        PillButton {
            Layout.alignment: Qt.AlignHCenter
            primary: false
            text: Plex.signingIn ? qsTr("Cancel") : qsTr("Connect to a server by address")
            onClicked: Plex.signingIn ? Plex.cancelSignIn() : Nav.show("settings")
        }
        PillButton {
            Layout.alignment: Qt.AlignHCenter
            primary: false
            text: qsTr("Use Apple Music instead")
            onClicked: Player.source = Player.AppleMusicSource
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 12
            visible: Plex.status === Plex.Error
            spacing: 10
            Icon { name: "warning"; size: 18; color: "#e0a030" }
            Text {
                Layout.maximumWidth: 380
                wrapMode: Text.WordWrap
                text: Plex.errorString
                color: Theme.textDim
            }
        }
    }

    // ---- Apple Music ----
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 64)
        spacing: 18
        visible: !Nav.plex

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/qt/qml/Kanzi/resources/kanzi.svg"
            sourceSize: Qt.size(112, 112)
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Welcome to Kanzi")
            color: Theme.text
            font.pixelSize: Theme.fontHero - 6
            font.weight: Font.Bold
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Sign in with your Apple ID to stream your library, playlists and recommendations. An Apple Music subscription is required for full playback.")
            color: Theme.textDim
            font.pixelSize: Theme.fontBody + 1
            lineHeight: 1.25
        }

        PillButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            text: Player.ready ? qsTr("Sign in to Apple Music") : qsTr("Starting player…")
            iconName: "artist"
            enabled: Player.ready
            onClicked: Player.authorize()
        }

        PillButton {
            Layout.alignment: Qt.AlignHCenter
            primary: false
            text: qsTr("Browse without signing in")
            enabled: AppleMusic.status === AppleMusic.Ready
            onClicked: Nav.show("browse")
        }

        PillButton {
            Layout.alignment: Qt.AlignHCenter
            primary: false
            text: qsTr("Use Plex instead")
            onClicked: Player.source = Player.PlexSource
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 12
            visible: AppleMusic.status !== AppleMusic.Ready || Player.errorString !== ""
            spacing: 10
            Spinner { running: AppleMusic.status === AppleMusic.Loading; size: 18 }
            Icon { visible: AppleMusic.status === AppleMusic.Error || Player.errorString !== ""; name: "warning"; size: 18; color: "#e0a030" }
            Text {
                Layout.maximumWidth: 380
                wrapMode: Text.WordWrap
                text: AppleMusic.status === AppleMusic.Loading ? qsTr("Connecting to Apple Music…")
                    : AppleMusic.status === AppleMusic.Error ? AppleMusic.errorString
                    : Player.errorString
                color: Theme.textDim
            }
            IconButton {
                visible: AppleMusic.status === AppleMusic.Error
                iconName: "repeat"
                iconSize: 16
                tip: qsTr("Retry")
                onClicked: AppleMusic.retry()
            }
        }
    }
}
