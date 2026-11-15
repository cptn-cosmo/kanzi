import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

ColumnLayout {
    id: root
    spacing: 12

    property var songs: []
    property string next: ""
    property bool loading: false

    function fetch(path) {
        loading = true
        AppleMusic.get(path, { limit: 100 }, (res, err) => {
            loading = false
            if (err)
                return Nav.toast(err.message)
            songs = songs.concat(res.data || [])
            next = res.next || ""
        })
    }

    // MusicKit queues get unwieldy past a few hundred entries; queue a window from the clicked song.
    function playFrom(index, shuffle) {
        const ids = songs.slice(index, index + 300).filter(s => s.attributes && s.attributes.playParams).map(s => s.id)
        if (ids.length)
            Player.playCollection("songs", ids, 0, shuffle)
    }

    Component.onCompleted: fetch("/v1/me/library/songs")

    PageHeader {
        Layout.fillWidth: true
        title: qsTr("Songs")
        subtitle: root.songs.length ? (root.next ? qsTr("%1+ songs").arg(root.songs.length) : qsTr("%1 songs").arg(root.songs.length)) : ""

        PillButton { text: qsTr("Play"); iconName: "play"; enabled: root.songs.length > 0; onClicked: root.playFrom(0, false) }
        PillButton { text: qsTr("Shuffle"); iconName: "shuffle"; primary: false; enabled: root.songs.length > 0; onClicked: root.playFrom(0, true) }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: root.songs
        leftMargin: Theme.gutter - 12
        rightMargin: Theme.gutter - 12
        bottomMargin: 24
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: 1000
        reuseItems: true
        ScrollBar.vertical: ScrollBar {}

        delegate: TrackRow {
            required property var modelData
            required property int index
            width: list.width - list.leftMargin - list.rightMargin
            track: modelData
            showArtwork: true
            showAlbum: true
            onActivated: root.playFrom(index, false)
        }

        onAtYEndChanged: if (atYEnd && root.next && !root.loading) root.fetch(root.next)

        footer: Item {
            width: list.width
            height: root.loading ? 64 : 0
            Spinner { anchors.centerIn: parent; running: root.loading }
        }
    }
}
