import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Rectangle {
    id: root

    implicitHeight: 88
    color: Theme.bar

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    readonly property var np: Player.nowPlaying

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16

        // ---- now playing ----
        RowLayout {
            Layout.preferredWidth: 1
            Layout.fillWidth: true
            spacing: 12

            Artwork {
                id: art
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                artwork: root.np.artwork || null
                pixelSize: 112
                radius: 7
                visible: Player.hasTrack

                HoverHandler { id: artHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: Nav.nowPlayingOpen = true }
                Rectangle {
                    anchors.fill: parent
                    radius: 7
                    color: Qt.rgba(0, 0, 0, 0.45)
                    opacity: artHover.hovered ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                    Icon { anchors.centerIn: parent; name: "chevronDown"; rotation: 180; color: "white"; size: 24 }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: Player.hasTrack
                Text {
                    Layout.fillWidth: true
                    text: root.np.title || ""
                    color: Theme.text
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: [root.np.artist, root.np.album].filter(s => !!s).join(" — ")
                    color: Theme.textDim
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontSmall + 1
                }
            }

            Text {
                visible: !Player.hasTrack
                Layout.fillWidth: true
                text: Player.ready ? qsTr("Nothing playing") : qsTr("Starting player…")
                color: Theme.textFaint
            }
        }

        // ---- transport ----
        ColumnLayout {
            Layout.preferredWidth: 1.3
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            spacing: 2

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                IconButton {
                    iconName: "shuffle"
                    iconSize: 18
                    active: Player.shuffle
                    color: Theme.textDim
                    enabled: Player.ready
                    tip: qsTr("Shuffle")
                    onClicked: Player.shuffle = !Player.shuffle
                }
                IconButton {
                    iconName: "previous"
                    iconSize: 22
                    enabled: Player.hasTrack
                    onClicked: Player.previous()
                }
                Item {
                    implicitWidth: 44
                    implicitHeight: 44
                    IconButton {
                        anchors.fill: parent
                        iconName: Player.playing ? "pause" : "play"
                        iconSize: 22
                        filled: true
                        enabled: Player.hasTrack
                        onClicked: Player.togglePlayPause()
                    }
                    Spinner {
                        anchors.centerIn: parent
                        size: 48
                        color: Theme.accent
                        running: Player.buffering && Player.hasTrack
                    }
                }
                IconButton {
                    iconName: "next"
                    iconSize: 22
                    enabled: Player.hasTrack && Player.canGoNext
                    onClicked: Player.next()
                }
                IconButton {
                    iconName: Player.repeatMode === Player.RepeatOne ? "repeatOne" : "repeat"
                    iconSize: 18
                    active: Player.repeatMode !== Player.RepeatNone
                    color: Theme.textDim
                    enabled: Player.ready
                    tip: qsTr("Repeat")
                    onClicked: Player.cycleRepeat()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text {
                    text: Theme.formatTime(seek.pressed ? seek.value : Player.position)
                    color: Theme.textFaint
                    font.pixelSize: 11
                    font.features: { "tnum": 1 }
                    Layout.preferredWidth: 40
                    horizontalAlignment: Text.AlignRight
                }
                SeekBar {
                    id: seek
                    Layout.fillWidth: true
                    to: Math.max(1, Player.duration)
                    externalValue: Player.position
                    enabled: Player.hasTrack
                    onCommitted: value => Player.seek(value)
                }
                Text {
                    text: "-" + Theme.formatTime(Math.max(0, Player.duration - (seek.pressed ? seek.value : Player.position)))
                    color: Theme.textFaint
                    font.pixelSize: 11
                    font.features: { "tnum": 1 }
                    Layout.preferredWidth: 40
                }
            }
        }

        // ---- extras ----
        RowLayout {
            Layout.preferredWidth: 1
            Layout.fillWidth: true
            spacing: 4

            Item { Layout.fillWidth: true }

            IconButton {
                iconName: "miniPlayer"
                iconSize: 18
                color: Theme.textDim
                tip: qsTr("Mini Player (Ctrl+M)")
                onClicked: Nav.miniPlayerOpen = true
            }
            IconButton {
                iconName: "lyrics"
                iconSize: 18
                color: Theme.textDim
                tip: qsTr("Lyrics")
                active: Nav.nowPlayingOpen && Nav.lyricsOpen
                enabled: Player.hasTrack
                onClicked: {
                    if (Nav.nowPlayingOpen && Nav.lyricsOpen) {
                        Nav.nowPlayingOpen = false
                    } else {
                        Nav.lyricsOpen = true
                        Nav.nowPlayingOpen = true
                    }
                }
            }
            IconButton {
                iconName: "queue"
                iconSize: 18
                color: Theme.textDim
                tip: qsTr("Up Next")
                active: Nav.queueOpen
                onClicked: Nav.queueOpen = !Nav.queueOpen
            }
            IconButton {
                iconName: Player.volume <= 0.001 ? "volumeOff" : Player.volume < 0.5 ? "volumeLow" : "volume"
                iconSize: 18
                color: Theme.textDim
                property real lastVolume: 1
                onClicked: {
                    if (Player.volume > 0) {
                        lastVolume = Player.volume
                        Player.volume = 0
                    } else {
                        Player.volume = lastVolume
                    }
                }
            }
            SeekBar {
                Layout.preferredWidth: 110
                to: 1
                live: true
                externalValue: Player.volume
                onMoved: Player.volume = value
                WheelHandler {
                    onWheel: event => Player.volume = Math.max(0, Math.min(1, Player.volume + event.angleDelta.y / 2400))
                }
            }
        }
    }
}
