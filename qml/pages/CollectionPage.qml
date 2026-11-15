import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Kanzi

// Album or playlist detail: header with artwork and actions, followed by the track list.
ListView {
    id: root

    property string kind: "album"   // album | playlist
    property string itemId
    property bool library: false
    property var preview: null      // the resource we were opened from, shown while loading

    property var resource: preview
    property var tracks: []
    property string tracksNext: ""
    property bool loading: true
    property string error: ""

    readonly property var a: resource && resource.attributes ? resource.attributes : ({})
    readonly property bool isAlbum: kind === "album"
    readonly property string collectionPath: library ? `/v1/me/library/${kind}s/${itemId}` : `/v1/catalog/{sf}/${kind}s/${itemId}`
    readonly property real totalMs: tracks.reduce((sum, t) => sum + ((t.attributes && t.attributes.durationInMillis) || 0), 0)
    readonly property bool multiDisc: isAlbum && tracks.some(t => t.attributes && t.attributes.discNumber > 1)

    function load() {
        loading = true
        AppleMusic.get(collectionPath, { include: "tracks,artists" }, (res, err) => {
            loading = false
            if (err) {
                error = err.message
                return
            }
            const r = res.data && res.data.length ? res.data[0] : null
            if (!r)
                return
            resource = r
            const rel = r.relationships && r.relationships.tracks
            tracks = rel ? (rel.data || []) : []
            tracksNext = rel && rel.next ? rel.next : ""
            if (library && !rel)
                fetchTracks(`${collectionPath}/tracks`)
            else if (tracksNext)
                fetchTracks(tracksNext)
        })
    }

    function fetchTracks(path) {
        AppleMusic.get(path, { limit: 100 }, (res, err) => {
            if (err)
                return
            tracks = tracks.concat(res.data || [])
            if (res.next)
                fetchTracks(res.next)
        })
    }

    function play(index, shuffle) {
        Player.playCollection(kind, itemId, index, shuffle)
    }

    Component.onCompleted: load()

    model: tracks
    clip: true
    leftMargin: Theme.gutter - 12
    rightMargin: Theme.gutter - 12
    bottomMargin: 40
    boundsBehavior: Flickable.StopAtBounds
    cacheBuffer: 1200
    ScrollBar.vertical: ScrollBar {}

    header: Item {
        width: root.width - root.leftMargin - root.rightMargin
        height: headerLayout.implicitHeight + 28

        // Soft wash of the artwork colour behind the header.
        Rectangle {
            x: -root.leftMargin
            width: root.width
            height: parent.height + 60
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.alpha(Theme.artColor(root.a.artwork, "transparent"), 0.35) }
                GradientStop { position: 1; color: "transparent" }
            }
        }

        ColumnLayout {
            id: headerLayout
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.topMargin: 16
                IconButton { iconName: "back"; iconSize: 22; onClicked: Nav.back() }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.topMargin: 8
                spacing: 28

                Artwork {
                    Layout.preferredWidth: Math.min(260, root.width * 0.28)
                    Layout.preferredHeight: Layout.preferredWidth
                    Layout.alignment: Qt.AlignBottom
                    artwork: root.a.artwork || null
                    pixelSize: 520
                    radius: Theme.radiusLarge
                    fallbackIcon: root.isAlbum ? "album" : "playlist"
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowBlur: 0.8
                        shadowOpacity: 0.35
                        shadowVerticalOffset: 8
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    spacing: 6

                    Text {
                        text: root.isAlbum ? (root.a.isSingle ? qsTr("Single") : qsTr("Album")) : qsTr("Playlist")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 0.6
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.a.name || ""
                        color: Theme.text
                        font.pixelSize: Theme.fontHero - 6
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    LinkText {
                        Layout.fillWidth: true
                        text: root.a.artistName || root.a.curatorName || ""
                        visible: text !== ""
                        color: Theme.accent
                        font.pixelSize: Theme.fontLarge + 2
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        linkEnabled: root.isAlbum && !!root.resource
                        onClicked: Nav.openArtistOf(root.resource)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: {
                            const parts = []
                            if (root.a.genreNames && root.a.genreNames.length)
                                parts.push(root.a.genreNames[0])
                            if (root.a.releaseDate)
                                parts.push(root.a.releaseDate.slice(0, 4))
                            if (root.tracks.length)
                                parts.push(root.tracks.length === 1 ? qsTr("1 song") : qsTr("%1 songs").arg(root.tracks.length))
                            if (root.totalMs > 0)
                                parts.push(Theme.formatLongDuration(root.totalMs))
                            return parts.join(" · ")
                        }
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall + 1
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        readonly property var notes: root.a.editorialNotes || root.a.description || null
                        visible: text !== ""
                        text: notes ? (notes.short || notes.standard || "").replace(/<[^>]+>/g, "") : ""
                        color: Theme.textDim
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontSmall + 1
                    }

                    RowLayout {
                        Layout.topMargin: 12
                        spacing: 10
                        PillButton {
                            text: qsTr("Play")
                            iconName: "play"
                            enabled: root.tracks.length > 0
                            onClicked: root.play(0, false)
                        }
                        PillButton {
                            text: qsTr("Shuffle")
                            iconName: "shuffle"
                            primary: false
                            enabled: root.tracks.length > 0
                            onClicked: root.play(0, true)
                        }
                        IconButton {
                            iconName: "more"
                            iconSize: 20
                            enabled: !!root.resource
                            onClicked: collectionMenu.popup()
                            Menu {
                                id: collectionMenu
                                MenuItem { text: qsTr("Play Next"); onTriggered: Nav.queueItem(root.resource, true) }
                                MenuItem { text: qsTr("Play Later"); onTriggered: Nav.queueItem(root.resource, false) }
                                MenuSeparator {}
                                MenuItem {
                                    text: qsTr("Copy Apple Music Link")
                                    enabled: Nav.canShare(root.resource)
                                    onTriggered: Nav.share(root.resource)
                                }
                            }
                        }
                    }
                }
            }

            Spinner {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
                running: root.loading && root.tracks.length === 0
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
                visible: root.error !== ""
                text: root.error
                color: Theme.textDim
            }
        }
    }

    section.property: multiDisc ? "attributes.discNumber" : ""
    section.delegate: Text {
        required property string section
        leftPadding: 12
        topPadding: 16
        bottomPadding: 6
        text: qsTr("Disc %1").arg(section)
        color: Theme.textDim
        font.weight: Font.DemiBold
    }

    delegate: TrackRow {
        required property var modelData
        required property int index
        width: root.width - root.leftMargin - root.rightMargin
        track: modelData
        number: root.isAlbum ? ((modelData.attributes && modelData.attributes.trackNumber) || index + 1) : 0
        showArtwork: !root.isAlbum
        showAlbum: !root.isAlbum && root.width > 900
        showArtist: !root.isAlbum || (modelData.attributes && modelData.attributes.artistName !== root.a.artistName)
        onActivated: root.play(index, false)
    }

    footer: Text {
        width: root.width - root.leftMargin - root.rightMargin
        visible: root.isAlbum && !!root.a.copyright
        text: root.a.copyright || ""
        leftPadding: 12
        topPadding: 20
        color: Theme.textFaint
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }
}
