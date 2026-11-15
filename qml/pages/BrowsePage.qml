import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Flickable {
    id: root

    property var topSongs: []
    property string topSongsTitle: qsTr("Top Songs")
    property var albumCharts: []
    property var playlistCharts: []
    property bool loading: false
    property string error: ""

    function load() {
        if (AppleMusic.status !== AppleMusic.Ready)
            return
        loading = true
        AppleMusic.get("/v1/catalog/{sf}/charts", { types: "songs,albums,playlists", limit: 30 }, (res, err) => {
            loading = false
            if (err) {
                error = err.message
                return
            }
            const r = res.results || {}
            const songs = r.songs && r.songs.length ? r.songs[0] : null
            if (songs) {
                topSongs = songs.data.slice(0, 20)
                topSongsTitle = songs.name || topSongsTitle
            }
            albumCharts = (r.albums || []).map(c => ({ title: c.name, items: c.data }))
            playlistCharts = (r.playlists || []).map(c => ({ title: c.name, items: c.data }))
        })
    }

    Component.onCompleted: load()
    Connections {
        target: AppleMusic
        function onStatusChanged() { root.load() }
        function onStorefrontChanged() { root.load() }
    }

    contentHeight: column.implicitHeight + 40
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: column
        width: root.width
        spacing: 34

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("Browse")
            subtitle: qsTr("What the world is listening to right now")
        }

        Spinner {
            Layout.alignment: Qt.AlignHCenter
            running: root.loading
        }
        Text {
            Layout.leftMargin: Theme.gutter
            visible: root.error !== ""
            text: root.error
            color: Theme.textDim
        }

        Repeater {
            model: root.playlistCharts
            Shelf {
                required property var modelData
                Layout.fillWidth: true
                title: modelData.title
                items: modelData.items
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.topSongs.length > 0
            spacing: 12

            RowLayout {
                Layout.leftMargin: Theme.gutter
                Layout.rightMargin: Theme.gutter
                Text {
                    Layout.fillWidth: true
                    text: root.topSongsTitle
                    color: Theme.text
                    font.pixelSize: 21
                    font.weight: Font.Bold
                }
                PillButton {
                    text: qsTr("Play")
                    iconName: "play"
                    onClicked: Player.playCollection("songs", root.topSongs.map(s => s.id), 0)
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.gutter - 12
                Layout.rightMargin: Theme.gutter - 12
                columns: root.width > 1100 ? 2 : 1
                columnSpacing: 16
                rowSpacing: 0

                Repeater {
                    model: root.topSongs
                    TrackRow {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        track: modelData
                        number: index + 1
                        showArtwork: true
                        onActivated: Player.playCollection("songs", root.topSongs.map(s => s.id), index)
                    }
                }
            }
        }

        Repeater {
            model: root.albumCharts
            Shelf {
                required property var modelData
                Layout.fillWidth: true
                title: modelData.title
                items: modelData.items
            }
        }
    }
}
