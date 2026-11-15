import QtQuick
import QtQuick.Controls
import Kanzi

// Time-synced lyrics for the current track. Click a line to jump to it.
Item {
    id: root

    property var lines: []
    property string status: "idle" // idle | loading | ready | none
    property string loadedFor: ""
    property bool synced: true
    readonly property string songId: Player.nowPlaying.catalogId || ""

    readonly property int activeIndex: {
        if (!synced)
            return -1
        const t = Player.position * 1000 + 250 // small lead so lines appear on the beat
        let idx = -1
        for (let i = 0; i < lines.length; ++i) {
            if (lines[i].begin <= t)
                idx = i
            else
                break
        }
        return idx
    }

    // Apple's web player gets lyrics from amp-api; the public API host doesn't
    // serve them to the web token. Line-synced first (that's what we display),
    // word-synced ("syllable") as a fallback for songs that only have that.
    readonly property var sources: [
        "https://amp-api.music.apple.com/v1/catalog/{sf}/songs/%1/lyrics",
        "https://amp-api.music.apple.com/v1/catalog/{sf}/songs/%1/syllable-lyrics",
        "/v1/catalog/{sf}/songs/%1/lyrics",
    ]

    function load() {
        if (!visible || songId === loadedFor)
            return
        loadedFor = songId
        lines = []
        if (!songId || !AppleMusic.authorized) {
            status = "none"
            return
        }
        status = "loading"
        fetchFrom(0, songId)
    }

    function fetchFrom(index, requested) {
        if (index >= sources.length) {
            status = "none"
            return
        }
        AppleMusic.get(sources[index].arg(requested), {}, (res, err) => {
            if (requested !== root.songId)
                return
            const ttml = !err && res.data && res.data.length && res.data[0].attributes ? res.data[0].attributes.ttml : ""
            const parsed = ttml ? AppleMusic.parseTtml(ttml).filter(l => l.text.length > 0) : []
            if (!parsed.length)
                return root.fetchFrom(index + 1, requested)
            root.synced = parsed.some(l => l.begin >= 0)
            root.lines = parsed
            root.status = "ready"
        })
    }

    onSongIdChanged: load()
    onVisibleChanged: load()

    ListView {
        id: list
        anchors.fill: parent
        model: root.lines
        spacing: 22
        clip: true
        visible: root.status === "ready"
        highlightFollowsCurrentItem: root.synced
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.3 + 60
        highlightRangeMode: ListView.ApplyRange
        highlightMoveDuration: 420
        highlightMoveVelocity: -1
        currentIndex: Math.max(0, root.activeIndex)
        boundsBehavior: Flickable.StopAtBounds
        topMargin: height * 0.3
        bottomMargin: height * 0.6
        interactive: true

        delegate: Text {
            id: line
            required property var modelData
            required property int index
            readonly property bool active: index === root.activeIndex
            readonly property bool past: index < root.activeIndex

            width: list.width - 24
            text: modelData.text
            wrapMode: Text.WordWrap
            color: "white"
            opacity: !root.synced ? 0.9 : active ? 1 : past ? 0.35 : 0.5
            font.pixelSize: 30
            font.weight: Font.Bold
            transformOrigin: Item.Left
            scale: active ? 1.0 : 0.96
            Behavior on opacity { NumberAnimation { duration: 260 } }
            Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

            HoverHandler { id: hh; cursorShape: Qt.PointingHandCursor }
            TapHandler { enabled: root.synced; onTapped: Player.seek(line.modelData.begin / 1000) }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.status === "none"
        text: AppleMusic.authorized ? qsTr("No lyrics available") : qsTr("Sign in to see lyrics")
        color: Qt.rgba(1, 1, 1, 0.6)
        font.pixelSize: Theme.fontLarge
    }

    Spinner {
        anchors.centerIn: parent
        running: root.status === "loading"
        color: "white"
    }
}
