import QtQuick
import QtQuick.Layouts
import Kanzi

// Library albums / artists / playlists.
ColumnLayout {
    id: root
    spacing: 16

    property string kind: "albums"
    property var items: []
    property var next: ""   // Apple: next page's path; Plex: next offset
    property bool loading: false

    readonly property string title: ({ albums: qsTr("Albums"), artists: qsTr("Artists"), playlists: qsTr("Playlists") })[kind]

    function fetch(path) {
        loading = true
        if (Nav.plex) {
            Plex.browse(kind, path || 0, (res, err) => {
                loading = false
                if (err)
                    return Nav.toast(err.message)
                items = items.concat(res.data || [])
                next = res.next > 0 ? res.next : ""
            })
            return
        }
        const query = { limit: 100 }
        if (kind === "artists")
            query.include = "catalog"
        AppleMusic.get(path, query, (res, err) => {
            loading = false
            if (err)
                return Nav.toast(err.message)
            items = items.concat(res.data || [])
            next = res.next || ""
        })
    }

    Component.onCompleted: fetch(Nav.plex ? 0 : `/v1/me/library/${kind}`)

    PageHeader {
        Layout.fillWidth: true
        title: root.title
        subtitle: root.items.length ? (root.next ? qsTr("%1+ items").arg(root.items.length) : qsTr("%1 items").arg(root.items.length)) : ""
    }

    MediaGrid {
        Layout.fillWidth: true
        Layout.fillHeight: true
        items: root.items
        loading: root.loading
        minCardWidth: root.kind === "artists" ? 150 : 170
        onLoadMore: if (root.next) root.fetch(root.next)
    }
}
