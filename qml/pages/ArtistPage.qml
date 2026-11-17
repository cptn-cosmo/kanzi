import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Kanzi

Flickable {
    id: root

    property string itemId
    property bool library: false
    property var preview: null

    property var artist: preview
    property var topSongs: []
    property var shelves: []
    property var libraryAlbums: []
    property bool loading: true
    property string error: ""

    readonly property var a: artist && artist.attributes ? artist.attributes : ({})
    readonly property var art: Nav.artworkOf(artist)

    readonly property var viewOrder: [
        "latest-release", "full-albums", "singles", "live-albums", "compilation-albums",
        "appears-on-albums", "featured-playlists", "similar-artists"
    ]

    function loadCatalog(id) {
        AppleMusic.get(`/v1/catalog/{sf}/artists/${id}`, {
            views: "top-songs," + viewOrder.join(","),
            extend: "artistBio,editorialArtwork"
        }, (res, err) => {
            if (err)
                return loadSeparately(id, err)
            loading = false
            const r = res.data && res.data.length ? res.data[0] : null
            if (!r)
                return
            artist = r
            applyViews(r.views || {})
        })
    }

    function applyViews(views) {
        topSongs = views["top-songs"] && views["top-songs"].data ? views["top-songs"].data.slice(0, 10) : []
        shelves = viewOrder.filter(k => views[k] && views[k].data && views[k].data.length)
                           .map(k => ({ title: (views[k].attributes && views[k].attributes.title) || viewTitles[k] || k, items: views[k].data }))
    }

    readonly property var viewTitles: ({
        "latest-release": qsTr("Latest Release"),
        "full-albums": qsTr("Albums"),
        "singles": qsTr("Singles & EPs"),
        "live-albums": qsTr("Live Albums"),
        "compilation-albums": qsTr("Compilations"),
        "appears-on-albums": qsTr("Appears On"),
        "featured-playlists": qsTr("Playlists"),
        "similar-artists": qsTr("Similar Artists"),
    })

    // Fallback when Apple rejects the combined request: fetch the artist and each
    // section on its own, so one unsupported view or extension can't blank the page.
    function loadSeparately(id, firstError) {
        console.warn("Kanzi: artist request failed (" + firstError.message + "); loading sections separately")
        const views = {}
        const names = ["top-songs"].concat(viewOrder)
        let pending = names.length + 1
        const done = () => {
            if (--pending === 0)
                loading = false
        }
        AppleMusic.get(`/v1/catalog/{sf}/artists/${id}`, {}, (res, err) => {
            const r = !err && res.data && res.data.length ? res.data[0] : null
            if (r)
                artist = r
            else if (err)
                error = err.message
            done()
        })
        for (const name of names) {
            AppleMusic.get(`/v1/catalog/{sf}/artists/${id}/view/${name}`, {}, (res, err) => {
                if (!err && res.data && res.data.length) {
                    views[name] = { data: res.data, attributes: res.attributes || (res.meta && res.meta.attributes) || {} }
                    applyViews(views)
                }
                done()
            })
        }
    }

    function load() {
        if (Nav.plex) {
            Plex.artist(itemId, (res, err) => {
                loading = false
                if (err)
                    return error = err.message
                artist = res.artist
                topSongs = res.topSongs || []
                shelves = res.shelves || []
            })
            return
        }
        if (!library)
            return loadCatalog(itemId)
        // Library artist: show what's in the library, then enrich from the catalog when linked.
        AppleMusic.get(`/v1/me/library/artists/${itemId}/albums`, { limit: 100 }, (res, err) => {
            if (!err)
                libraryAlbums = res.data || []
        })
        AppleMusic.get(`/v1/me/library/artists/${itemId}`, { include: "catalog" }, (res, err) => {
            const r = !err && res.data && res.data.length ? res.data[0] : null
            const cat = r && r.relationships && r.relationships.catalog ? r.relationships.catalog.data : []
            if (cat && cat.length)
                loadCatalog(cat[0].id)
            else
                loading = false
        })
    }

    Component.onCompleted: load()

    contentHeight: column.implicitHeight + 40
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: column
        width: root.width
        spacing: 34

        // ---- hero ----
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 300

            Rectangle {
                anchors.fill: parent
                color: Theme.artColor(root.art, Theme.sidebar)
            }
            Image {
                id: heroSrc
                anchors.fill: parent
                visible: false
                source: root.art ? AppleMusic.artworkUrl(root.art, 300) : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
            MultiEffect {
                anchors.fill: parent
                source: heroSrc
                blurEnabled: true
                blur: 1
                blurMax: 64
                saturation: 0.2
            }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.1) }
                    GradientStop { position: 1; color: Theme.bg }
                }
            }

            IconButton {
                x: Theme.gutter - 10
                y: 16
                iconName: "back"
                iconSize: 22
                color: "white"
                onClicked: Nav.back()
            }

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: Theme.gutter
                anchors.rightMargin: Theme.gutter
                anchors.bottomMargin: 12
                spacing: 24

                Artwork {
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 170
                    artwork: root.art
                    pixelSize: 340
                    circle: true
                    fallbackIcon: "artist"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: root.a.name || ""
                        color: Theme.text
                        font.pixelSize: Theme.fontHero + 8
                        font.weight: Font.Black
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: root.a.genreNames ? root.a.genreNames.join(", ") : ""
                        color: Theme.textDim
                    }
                    RowLayout {
                        spacing: 10
                        PillButton {
                            text: qsTr("Play")
                            iconName: "play"
                            enabled: root.topSongs.length > 0 || (Nav.plex && !!root.artist)
                            onClicked: Nav.plex ? Plex.play(root.artist, false)
                                                : Player.playCollection("songs", root.topSongs.map(s => s.id), 0)
                        }
                        PillButton {
                            text: qsTr("Shuffle")
                            iconName: "shuffle"
                            primary: false
                            enabled: root.topSongs.length > 0 || (Nav.plex && !!root.artist)
                            onClicked: Nav.plex ? Plex.play(root.artist, true)
                                                : Player.playCollection("songs", root.topSongs.map(s => s.id), 0, true)
                        }
                    }
                }
            }
        }

        Spinner {
            Layout.alignment: Qt.AlignHCenter
            running: root.loading
        }
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.gutter
            Layout.rightMargin: Theme.gutter
            visible: root.error !== ""
            text: root.error
            color: Theme.textDim
            wrapMode: Text.Wrap
        }

        // ---- top songs ----
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.topSongs.length > 0
            spacing: 10
            Text {
                Layout.leftMargin: Theme.gutter
                text: qsTr("Top Songs")
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
                    model: root.topSongs
                    TrackRow {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        track: modelData
                        showArtwork: true
                        showArtist: false
                        onActivated: Nav.plex ? Plex.playTracks(root.topSongs, index)
                                              : Player.playCollection("songs", root.topSongs.map(s => s.id), index)
                    }
                }
            }
        }

        Shelf {
            Layout.fillWidth: true
            title: qsTr("In Your Library")
            items: root.libraryAlbums
        }

        Repeater {
            model: root.shelves
            Shelf {
                required property var modelData
                Layout.fillWidth: true
                title: modelData.title
                items: modelData.items
                cardWidth: modelData.items.length && Nav.typeOf(modelData.items[0]) === "artists" ? 150 : 176
            }
        }

        // ---- about ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.gutter
            Layout.rightMargin: Theme.gutter
            visible: !!root.a.artistBio
            spacing: 10
            Text {
                text: qsTr("About")
                color: Theme.text
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 820
                text: (root.a.artistBio || "").replace(/<[^>]+>/g, "")
                color: Theme.textDim
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }
        }
    }
}
