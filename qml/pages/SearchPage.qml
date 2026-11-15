import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Flickable {
    id: root

    property string scope: "catalog" // catalog | library
    property var songs: []
    property var albums: []
    property var artists: []
    property var playlists: []
    property bool loading: false
    property int generation: 0
    readonly property string term: Nav.searchTerm.trim()
    readonly property bool empty: !songs.length && !albums.length && !artists.length && !playlists.length

    function run() {
        const gen = ++generation
        if (!term || AppleMusic.status !== AppleMusic.Ready) {
            songs = albums = artists = playlists = []
            return
        }
        loading = true
        const library = scope === "library"
        const path = library ? "/v1/me/library/search" : "/v1/catalog/{sf}/search"
        const types = library ? "library-songs,library-albums,library-artists,library-playlists"
                              : "songs,albums,artists,playlists"
        AppleMusic.get(path, { term: term, types: types, limit: 25 }, (res, err) => {
            if (gen !== root.generation)
                return
            loading = false
            if (err)
                return Nav.toast(err.message)
            const r = res.results || {}
            const pick = key => (r[library ? "library-" + key : key] || {}).data || []
            songs = pick("songs")
            albums = pick("albums")
            artists = pick("artists")
            playlists = pick("playlists")
        })
    }

    onTermChanged: run()
    onScopeChanged: run()
    Component.onCompleted: run()

    contentHeight: column.implicitHeight + 40
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: column
        width: root.width
        spacing: 30

        PageHeader {
            Layout.fillWidth: true
            title: root.term ? qsTr("Results for “%1”").arg(root.term) : qsTr("Search")

            Rectangle {
                visible: AppleMusic.authorized
                implicitWidth: scopeRow.implicitWidth + 8
                implicitHeight: 34
                radius: height / 2
                color: Theme.hover
                RowLayout {
                    id: scopeRow
                    anchors.centerIn: parent
                    spacing: 0
                    Repeater {
                        model: [{ key: "catalog", label: qsTr("Apple Music") }, { key: "library", label: qsTr("Library") }]
                        AbstractButton {
                            id: seg
                            required property var modelData
                            implicitHeight: 28
                            implicitWidth: segLabel.implicitWidth + 28
                            onClicked: root.scope = modelData.key
                            background: Rectangle {
                                radius: height / 2
                                color: root.scope === seg.modelData.key ? Theme.surface : "transparent"
                            }
                            contentItem: Text {
                                id: segLabel
                                text: seg.modelData.label
                                color: Theme.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.weight: root.scope === seg.modelData.key ? Font.DemiBold : Font.Normal
                            }
                        }
                    }
                }
            }
        }

        Spinner {
            Layout.alignment: Qt.AlignHCenter
            running: root.loading && root.empty
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 80
            visible: !root.loading && root.empty
            text: root.term ? qsTr("No results") : qsTr("Search for songs, albums, artists and playlists.\nPress Ctrl+F from anywhere.")
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textFaint
            font.pixelSize: Theme.fontLarge
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.songs.length > 0
            spacing: 10
            Text {
                Layout.leftMargin: Theme.gutter
                text: qsTr("Songs")
                color: Theme.text
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.gutter - 12
                Layout.rightMargin: Theme.gutter - 12
                columns: root.width > 1100 ? 2 : 1
                columnSpacing: 16
                rowSpacing: 0
                Repeater {
                    model: root.songs.slice(0, 12)
                    TrackRow {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        track: modelData
                        showArtwork: true
                        onActivated: Player.playCollection("songs", root.songs.map(s => s.id), index)
                    }
                }
            }
        }

        Shelf { Layout.fillWidth: true; title: qsTr("Artists"); items: root.artists; cardWidth: 150 }
        Shelf { Layout.fillWidth: true; title: qsTr("Albums"); items: root.albums }
        Shelf { Layout.fillWidth: true; title: qsTr("Playlists"); items: root.playlists }
    }
}
