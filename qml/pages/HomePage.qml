import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

ListView {
    id: root

    property var recentlyPlayed: []
    property var heavyRotation: []
    property var recommendations: []
    property int pending: 0
    property string error: ""

    readonly property var shelves: {
        const out = []
        if (recentlyPlayed.length)
            out.push({ title: qsTr("Recently Played"), items: recentlyPlayed })
        if (heavyRotation.length)
            out.push({ title: qsTr("Heavy Rotation"), items: heavyRotation })
        return out.concat(recommendations)
    }

    function greeting() {
        const h = new Date().getHours()
        return h < 5 ? qsTr("Good night") : h < 12 ? qsTr("Good morning") : h < 18 ? qsTr("Good afternoon") : qsTr("Good evening")
    }

    function load() {
        if (AppleMusic.status !== AppleMusic.Ready || !AppleMusic.authorized)
            return
        pending = 3
        error = ""
        AppleMusic.get("/v1/me/recent/played", { limit: 20 }, (res, err) => {
            pending--
            if (!err) recentlyPlayed = res.data || []
            else error = err.message
        })
        AppleMusic.get("/v1/me/history/heavy-rotation", { limit: 20 }, (res, err) => {
            pending--
            if (!err) heavyRotation = res.data || []
        })
        AppleMusic.get("/v1/me/recommendations", { limit: 20 }, (res, err) => {
            pending--
            if (err)
                return
            recommendations = (res.data || []).map(rec => ({
                title: rec.attributes && rec.attributes.title ? rec.attributes.title.stringForDisplay : "",
                items: rec.relationships && rec.relationships.contents ? rec.relationships.contents.data : []
            })).filter(s => s.items.length > 0)
        })
    }

    Component.onCompleted: load()
    Connections {
        target: AppleMusic
        function onStatusChanged() { root.load() }
        function onStorefrontChanged() { root.load() }
    }

    model: shelves
    spacing: 34
    bottomMargin: 40
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    cacheBuffer: 1200
    ScrollBar.vertical: ScrollBar {}

    header: ColumnLayout {
        width: root.width
        spacing: 0
        PageHeader {
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            title: root.greeting()
            subtitle: qsTr("Made for you, from Apple Music")
        }
        Spinner {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 60
            running: root.pending > 0 && root.shelves.length === 0
        }
        Text {
            Layout.leftMargin: Theme.gutter
            visible: root.error !== "" && root.shelves.length === 0
            text: qsTr("Couldn't load your recommendations: %1").arg(root.error)
            color: Theme.textDim
        }
    }

    delegate: Shelf {
        required property var modelData
        width: root.width
        title: modelData.title
        items: modelData.items
    }
}
