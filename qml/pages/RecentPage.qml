import QtQuick
import QtQuick.Layouts
import Kanzi

ColumnLayout {
    id: root
    spacing: 16

    property var items: []
    property string next: ""
    property bool loading: false

    function fetch(path) {
        loading = true
        AppleMusic.get(path, { limit: 25 }, (res, err) => {
            loading = false
            if (err)
                return Nav.toast(err.message)
            items = items.concat(res.data || [])
            next = res.next || ""
        })
    }

    Component.onCompleted: fetch("/v1/me/library/recently-added")

    PageHeader {
        Layout.fillWidth: true
        title: qsTr("Recently Added")
    }

    MediaGrid {
        Layout.fillWidth: true
        Layout.fillHeight: true
        items: root.items
        loading: root.loading
        onLoadMore: if (root.next) root.fetch(root.next)
    }
}
