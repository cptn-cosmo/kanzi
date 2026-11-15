import QtQuick
import QtQuick.Controls
import Kanzi

// Responsive grid of MediaCards with infinite scrolling via `loadMore`.
GridView {
    id: root

    property var items: []
    property int minCardWidth: 170
    property bool loading: false
    signal loadMore()

    readonly property int columns: Math.max(2, Math.floor((width - 2 * Theme.gutter + 20) / (minCardWidth + 20)))
    readonly property real cardWidth: (width - 2 * Theme.gutter - (columns - 1) * 20) / columns

    leftMargin: Theme.gutter
    rightMargin: Theme.gutter
    bottomMargin: 32
    cellWidth: cardWidth + 20
    cellHeight: cardWidth + 74
    clip: true
    model: items
    cacheBuffer: 800
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    delegate: MediaCard {
        required property var modelData
        width: root.cardWidth
        item: modelData
    }

    onAtYEndChanged: if (atYEnd && !loading && count > 0) loadMore()

    footer: Item {
        width: root.width - 2 * Theme.gutter
        height: root.loading ? 64 : 0
        Spinner { anchors.centerIn: parent; running: root.loading }
    }
}
