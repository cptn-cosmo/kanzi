import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Kanzi

// Full-window now playing view with a blurred artwork backdrop, lyrics and up next.
Item {
    id: root

    readonly property var np: Player.nowPlaying
    readonly property bool open: Nav.nowPlayingOpen && Player.hasTrack

    visible: opacity > 0
    opacity: open ? 1 : 0
    y: open ? 0 : 40
    z: 50
    Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }

    onOpenChanged: if (!open) Nav.nowPlayingOpen = false

    // ---- backdrop ----
    Rectangle {
        anchors.fill: parent
        color: root.np.artworkColor ? "#" + root.np.artworkColor : "#1a1a1e"
    }
    Image {
        id: backdropSrc
        anchors.fill: parent
        visible: false
        source: root.np.artwork ? AppleMusic.artworkUrl(root.np.artwork, 300) : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }
    MultiEffect {
        anchors.fill: parent
        source: backdropSrc
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.25
        brightness: -0.08
        scale: 1.25
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.35) }
            GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.65) }
        }
    }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }

    IconButton {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 20
        iconName: "chevronDown"
        iconSize: 28
        color: "white"
        onClicked: Nav.nowPlayingOpen = false
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 64
        anchors.topMargin: 72
        spacing: 64

        // ---- left: artwork + transport ----
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(520, root.height - 260)
            Layout.maximumWidth: 520
            Layout.alignment: Qt.AlignVCenter
            spacing: 18

            Item { Layout.fillHeight: true }

            Artwork {
                id: bigArt
                Layout.fillWidth: true
                Layout.preferredHeight: width
                artwork: root.np.artwork || null
                pixelSize: 1000
                radius: Theme.radiusLarge
                scale: Player.playing ? 1 : 0.92
                Behavior on scale { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: 1.0
                    shadowOpacity: 0.5
                    shadowVerticalOffset: 16
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: root.np.title || ""
                        color: "white"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: [root.np.artist, root.np.album].filter(s => !!s).join(" — ")
                        color: Qt.rgba(1, 1, 1, 0.7)
                        font.pixelSize: Theme.fontLarge
                        elide: Text.ElideRight
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                SeekBar {
                    id: seek
                    Layout.fillWidth: true
                    to: Math.max(1, Player.duration)
                    externalValue: Player.position
                    trackColor: Qt.rgba(1, 1, 1, 0.22)
                    fillColor: "white"
                    onCommitted: value => Player.seek(value)
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: Theme.formatTime(seek.pressed ? seek.value : Player.position)
                        color: Qt.rgba(1, 1, 1, 0.6); font.pixelSize: 12
                        font.features: { "tnum": 1 }
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "-" + Theme.formatTime(Math.max(0, Player.duration - (seek.pressed ? seek.value : Player.position)))
                        color: Qt.rgba(1, 1, 1, 0.6); font.pixelSize: 12
                        font.features: { "tnum": 1 }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 22
                IconButton {
                    iconName: "shuffle"; iconSize: 20; color: "white"
                    active: Player.shuffle
                    onClicked: Player.shuffle = !Player.shuffle
                }
                IconButton { iconName: "previous"; iconSize: 32; color: "white"; onClicked: Player.previous() }
                IconButton {
                    iconName: Player.playing ? "pause" : "play"
                    iconSize: 42
                    color: "white"
                    onClicked: Player.togglePlayPause()
                }
                IconButton {
                    iconName: "next"; iconSize: 32; color: "white"
                    enabled: Player.canGoNext
                    onClicked: Player.next()
                }
                IconButton {
                    iconName: Player.repeatMode === Player.RepeatOne ? "repeatOne" : "repeat"
                    iconSize: 20; color: "white"
                    active: Player.repeatMode !== Player.RepeatNone
                    onClicked: Player.cycleRepeat()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Icon { name: "volumeLow"; size: 16; color: Qt.rgba(1, 1, 1, 0.6) }
                SeekBar {
                    Layout.fillWidth: true
                    to: 1
                    externalValue: Player.volume
                    trackColor: Qt.rgba(1, 1, 1, 0.22)
                    fillColor: "white"
                    onMoved: Player.volume = value
                }
                Icon { name: "volume"; size: 16; color: Qt.rgba(1, 1, 1, 0.6) }
            }

            Item { Layout.fillHeight: true }
        }

        // ---- right: lyrics / up next ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            RowLayout {
                spacing: 8
                component Tab: AbstractButton {
                    id: tab
                    property bool selected
                    hoverEnabled: true
                    implicitHeight: 32
                    implicitWidth: label.implicitWidth + 28
                    background: Rectangle {
                        radius: height / 2
                        color: tab.selected ? Qt.rgba(1, 1, 1, 0.22) : tab.hovered ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                    }
                    contentItem: Text {
                        id: label
                        text: tab.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.weight: Font.DemiBold
                    }
                }
                Tab { text: qsTr("Lyrics"); selected: Nav.lyricsOpen; onClicked: Nav.lyricsOpen = true }
                Tab { text: qsTr("Up Next"); selected: !Nav.lyricsOpen; onClicked: Nav.lyricsOpen = false }
            }

            LyricsView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: Nav.lyricsOpen && root.open
            }

            ListView {
                id: upNext
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !Nav.lyricsOpen
                clip: true
                spacing: 4
                model: Player.queue
                boundsBehavior: Flickable.StopAtBounds
                delegate: ItemDelegate {
                    id: qrow
                    required property var modelData
                    required property int index
                    readonly property bool current: index === Player.queuePosition
                    width: upNext.width
                    implicitHeight: 56
                    visible: index >= Player.queuePosition
                    height: visible ? implicitHeight : 0
                    hoverEnabled: true
                    onClicked: Player.jumpTo(index)
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: qrow.current ? Qt.rgba(1, 1, 1, 0.16) : qrow.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: 12
                        Artwork {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                            artwork: qrow.modelData.artwork || null
                            pixelSize: 84
                            radius: 5
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text { Layout.fillWidth: true; text: qrow.modelData.title; color: "white"; elide: Text.ElideRight; font.weight: Font.DemiBold }
                            Text { Layout.fillWidth: true; text: qrow.modelData.artist; color: Qt.rgba(1, 1, 1, 0.65); elide: Text.ElideRight; font.pixelSize: Theme.fontSmall }
                        }
                        Text { text: Theme.formatTime(qrow.modelData.duration); color: Qt.rgba(1, 1, 1, 0.55); font.pixelSize: Theme.fontSmall }
                    }
                }
            }
        }
    }
}
