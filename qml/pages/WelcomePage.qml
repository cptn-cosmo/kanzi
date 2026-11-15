import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Item {
    id: root

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 64)
        spacing: 18

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
