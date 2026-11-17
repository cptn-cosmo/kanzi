pragma Singleton
import QtQuick
import QtQuick.Controls
import Kanzi

// App-wide navigation state and helpers for turning API resources into actions.
QtObject {
    id: nav

    property var stack: null
    property string section: "home"
    property string searchTerm: ""
    property bool nowPlayingOpen: false
    property bool queueOpen: false
    property bool lyricsOpen: true
    property bool miniPlayerOpen: false

    // The active music source, and whether it's signed in (decides home vs. welcome).
    readonly property bool plex: Player.source === Player.PlexSource
    readonly property bool signedIn: plex ? Plex.signedIn : AppleMusic.authorized

    signal toast(string message)
    signal focusSearch()

    readonly property var sections: ({
        home: "pages/HomePage.qml",
        browse: "pages/BrowsePage.qml",
        recent: "pages/RecentPage.qml",
        search: "pages/SearchPage.qml",
        albums: "pages/LibraryGridPage.qml",
        artists: "pages/LibraryGridPage.qml",
        playlists: "pages/LibraryGridPage.qml",
        songs: "pages/LibrarySongsPage.qml",
        welcome: "pages/WelcomePage.qml",
        settings: "pages/SettingsPage.qml",
    })

    function show(key, props) {
        if (!stack)
            return
        section = key
        nowPlayingOpen = false
        const p = props || {}
        if (key === "albums" || key === "artists" || key === "playlists")
            p.kind = key
        stack.replace(null, Qt.resolvedUrl(sections[key]), p, StackView.Immediate)
    }

    function push(page, props) {
        nowPlayingOpen = false
        stack.push(Qt.resolvedUrl(page), props || {})
    }

    function back() {
        if (stack && stack.depth > 1)
            stack.pop()
    }

    function search(term) {
        searchTerm = term
        if (section !== "search" || stack.depth > 1)
            show("search")
    }

    // ---- resource helpers ----

    function typeOf(item) { return item && item.type ? item.type : "" }
    function isPlex(item) { return !!item && item.source === "plex" }
    function isLibrary(item) { return typeOf(item).startsWith("library-") }
    function attrs(item) { return item && item.attributes ? item.attributes : {} }
    function titleOf(item) { return attrs(item).name || "" }

    function artworkOf(item) {
        const a = attrs(item)
        if (a.artwork)
            return a.artwork
        // Library artists carry their picture on the catalog relationship.
        const rel = item && item.relationships
        if (rel && rel.catalog && rel.catalog.data && rel.catalog.data.length)
            return attrs(rel.catalog.data[0]).artwork || null
        return null
    }

    function subtitleOf(item) {
        const a = attrs(item)
        const t = typeOf(item).replace("library-", "")
        if (a.artistName)
            return a.artistName
        if (a.curatorName)
            return a.curatorName
        if (t === "playlists")
            return qsTr("Playlist")
        if (t === "stations")
            return qsTr("Station")
        if (t === "artists")
            return qsTr("Artist")
        return ""
    }

    // The MusicKit queue descriptor key for a resource, or "" when it isn't playable.
    function playKind(item) {
        const t = typeOf(item).replace("library-", "")
        return ({ albums: "album", playlists: "playlist", songs: "song", stations: "station" })[t] || ""
    }

    function canOpen(item) {
        const t = typeOf(item).replace("library-", "")
        return t === "albums" || t === "playlists" || t === "artists"
    }

    function playItem(item, shuffle) {
        const kind = playKind(item)
        if (!kind)
            return openItem(item)
        if (!Player.ready)
            return toast(qsTr("The player is still starting…"))
        if (isPlex(item))
            return Plex.play(item, !!shuffle)
        Player.playCollection(kind, item.id, 0, !!shuffle)
    }

    function queueItem(item, next) {
        const kind = playKind(item)
        if (!kind)
            return
        if (isPlex(item))
            Plex.enqueue(item, !!next)
        else if (next)
            Player.playNext(kind, item.id)
        else
            Player.playLater(kind, item.id)
        toast(next ? qsTr("Playing next: %1").arg(titleOf(item)) : qsTr("Added to queue: %1").arg(titleOf(item)))
    }

    function openItem(item) {
        const t = typeOf(item)
        const library = isLibrary(item)
        switch (t.replace("library-", "")) {
        case "albums":
            return push("pages/CollectionPage.qml", { kind: "album", itemId: item.id, library: library, preview: item })
        case "playlists":
            return push("pages/CollectionPage.qml", { kind: "playlist", itemId: item.id, library: library, preview: item })
        case "artists":
            return push("pages/ArtistPage.qml", { itemId: item.id, library: library, preview: item })
        case "songs":
        case "stations":
            return playItem(item)
        }
    }

    // ---- artist links & sharing ----
    // These accept API resources as well as Player.nowPlaying / queue entries.

    function catalogIdOf(item) {
        if (!item || isPlex(item))
            return ""
        if (item.catalogId)
            return String(item.catalogId)
        const pp = attrs(item).playParams || {}
        if (pp.catalogId)
            return String(pp.catalogId)
        if (pp.globalId)
            return String(pp.globalId) // library playlists shared from the catalog
        const id = String(item.id || "")
        return !isLibrary(item) && /^(\d+|pl\.[\w-]+|ra\.[\w-]+)$/.test(id) ? id : ""
    }

    function shareUrlOf(item) {
        if (!item || isPlex(item))
            return ""
        const a = attrs(item)
        if (a.url)
            return a.url
        if (item.url)
            return item.url
        const id = catalogIdOf(item)
        if (!id)
            return ""
        const t = typeOf(item).replace("library-", "")
        const path = ({ albums: "album", playlists: "playlist", artists: "artist", stations: "station" })[t] || "song"
        return `https://music.apple.com/${AppleMusic.storefront || "us"}/${path}/${id}`
    }

    function share(item) {
        const url = shareUrlOf(item)
        if (!url)
            return toast(qsTr("This isn't on Apple Music, so there's no link to share"))
        AppleMusic.copyToClipboard(url)
        toast(qsTr("Link copied to clipboard"))
    }

    function canShare(item) { return shareUrlOf(item) !== "" }

    // Opens the (first) artist of a song or album, looking it up when the item doesn't carry it.
    function openArtistOf(item) {
        if (!item)
            return
        const rel = item.relationships
        if (rel && rel.artists && rel.artists.data && rel.artists.data.length)
            return openItem(rel.artists.data[0])
        if (isPlex(item))
            return toast(qsTr("Couldn't find this artist"))
        let t = typeOf(item).replace("library-", "")
        if (t !== "albums" && t !== "music-videos")
            t = "songs"
        const id = catalogIdOf(item)
        const path = id ? `/v1/catalog/{sf}/${t}/${id}/artists`
                   : item.id ? `/v1/me/library/${t}/${item.id}/artists` : ""
        if (!path)
            return
        AppleMusic.get(path, {}, (res, err) => {
            const found = !err && res.data ? res.data : []
            if (found.length)
                openItem(found[0])
            else
                toast(qsTr("Couldn't find this artist on Apple Music"))
        })
    }
}
