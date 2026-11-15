import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

// One song in a track list. Emits `activated` on double-click (or Enter).
ItemDelegate {
    id: root

    property var track: null
    property int number: 0
    property bool showArtwork: false
    property bool showAlbum: false
    property bool showArtist: true

    readonly property var a: track && track.attributes ? track.attributes : ({})
    readonly property string catalogId: a.playParams && a.playParams.catalogId ? String(a.playParams.catalogId) : ""
    readonly property bool current: Player.hasTrack && track
                                    && (Player.nowPlaying.id === track.id
                                        || (catalogId !== "" && Player.nowPlaying.catalogId === catalogId))
    readonly property bool unavailable: track && !a.playParams

    signal activated()

    implicitHeight: showArtwork ? 56 : 46
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    padding: 0
    leftPadding: 12
    rightPadding: 8
    opacity: unavailable ? 0.45 : 1

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.pressed ? Theme.pressed : root.hovered ? Theme.hover : "transparent"
    }

    onDoubleClicked: if (!unavailable) activated()

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: if (!root.unavailable) menu.popup()
    }

    contentItem: RowLayout {
        spacing: 12

        Item {
            Layout.preferredWidth: root.showArtwork ? 40 : 28
            Layout.preferredHeight: root.showArtwork ? 40 : 28

            Artwork {
                anchors.fill: parent
                visible: root.showArtwork
                artwork: root.a.artwork || null
                pixelSize: 80
                radius: 5
            }

            Rectangle {
                anchors.fill: parent
                visible: root.showArtwork && (root.current || root.hovered)
                radius: 5
                color: Qt.rgba(0, 0, 0, 0.45)
            }

            Text {
                anchors.centerIn: parent
                visible: !root.showArtwork && !root.current && !root.hovered
                text: root.number > 0 ? root.number : ""
                color: Theme.textFaint
                font.pixelSize: Theme.fontBody
                font.features: { "tnum": 1 }
            }

            Icon {
                anchors.centerIn: parent
                visible: root.hovered && !root.unavailable
                name: root.current && Player.playing ? "pause" : "play"
                size: 16
                color: root.showArtwork ? "white" : Theme.text
                TapHandler {
                    onTapped: root.current ? Player.togglePlayPause() : root.activated()
                }
            }

            // Animated equaliser bars for the current track.
            Row {
                anchors.centerIn: parent
                visible: root.current && !root.hovered
                spacing: 2
                height: 12
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        width: 3
                        radius: 1
                        anchors.bottom: parent.bottom
                        color: root.showArtwork ? "white" : Theme.accent
                        height: 4
                        SequentialAnimation on height {
                            running: Player.playing && root.current
                            loops: Animation.Infinite
                            NumberAnimation { to: 12; duration: 260 + index * 90; easing.type: Easing.InOutSine }
                            NumberAnimation { to: 4; duration: 260 + index * 90; easing.type: Easing.InOutSine }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 3
            spacing: 1

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    Layout.fillWidth: true
                    text: root.a.name || ""
                    color: root.current ? Theme.accent : Theme.text
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontBody
                    font.weight: root.current ? Font.DemiBold : Font.Normal
                }
                Icon {
                    visible: root.a.contentRating === "explicit"
                    name: "explicit"
                    size: 14
                    color: Theme.textFaint
                }
            }
            LinkText {
                Layout.fillWidth: true
                visible: root.showArtist && text !== ""
                onClicked: Nav.openArtistOf(root.track)
                text: root.a.artistName || ""
                color: Theme.textDim
                elide: Text.ElideRight
                font.pixelSize: Theme.fontSmall + 1
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.preferredWidth: 2
            visible: root.showAlbum
            text: root.a.albumName || ""
            color: Theme.textDim
            elide: Text.ElideRight
            font.pixelSize: Theme.fontSmall + 1
        }

        Text {
            Layout.preferredWidth: 48
            horizontalAlignment: Text.AlignRight
            text: root.a.durationInMillis ? Theme.formatTime(root.a.durationInMillis / 1000) : ""
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall + 1
            font.features: { "tnum": 1 }
        }

        IconButton {
            iconName: "more"
            iconSize: 16
            opacity: root.hovered ? 1 : 0
            enabled: !root.unavailable
            onClicked: menu.popup()
        }
    }

    Menu {
        id: menu
        MenuItem { text: qsTr("Play"); onTriggered: root.activated() }
        MenuSeparator {}
        MenuItem { text: qsTr("Play Next"); onTriggered: Nav.queueItem(root.track, true) }
        MenuItem { text: qsTr("Play Later"); onTriggered: Nav.queueItem(root.track, false) }
        MenuSeparator {}
        MenuItem { text: qsTr("Go to Artist"); onTriggered: Nav.openArtistOf(root.track) }
        MenuItem {
            text: qsTr("Copy Apple Music Link")
            enabled: Nav.canShare(root.track)
            onTriggered: Nav.share(root.track)
        }
    }
}
