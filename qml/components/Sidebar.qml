import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Rectangle {
    id: root
    color: Theme.sidebar
    clip: true

    property var playlists: []

    // Collapsed shows icons only. Focusing search (click or Ctrl+F) widens it again
    // until the search field loses focus. Icons keep the same x in both modes, so
    // only the labels move while the width animates.
    property bool collapsed: false
    readonly property bool compact: collapsed && !search.activeFocus
    implicitWidth: compact ? 64 : 252
    Behavior on implicitWidth { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    Settings {
        category: "sidebar"
        property alias collapsed: root.collapsed
    }

    // Leaving the search page drops the temporary expansion.
    Connections {
        target: Nav.stack
        function onCurrentItemChanged() {
            if (root.collapsed && (Nav.section !== "search" || Nav.stack.depth > 1))
                search.focus = false
        }
    }

    function loadPlaylists() {
        if (Nav.plex) {
            playlists = []
            if (Plex.ready)
                Plex.browse("playlists", 0, (res, err) => {
                    if (!err && Nav.plex)
                        root.playlists = res.data || []
                })
            return
        }
        if (!AppleMusic.authorized) {
            playlists = []
            return
        }
        AppleMusic.get("/v1/me/library/playlists", { limit: 100 }, (res, err) => {
            if (!err && res.data && !Nav.plex)
                root.playlists = res.data
        })
    }

    Connections {
        target: AppleMusic
        function onUserTokenChanged() { root.loadPlaylists() }
        function onStatusChanged() { if (AppleMusic.status === AppleMusic.Ready) root.loadPlaylists() }
    }
    Connections {
        target: Plex
        function onStatusChanged() { root.loadPlaylists() }
        function onSectionChanged() { root.loadPlaylists() }
    }
    Connections {
        target: Player
        function onSourceChanged() { root.loadPlaylists() }
    }
    Component.onCompleted: loadPlaylists()

    readonly property string statusText: {
        if (Nav.plex) {
            switch (Plex.status) {
            case Plex.Ready: return qsTr("Plex · %1").arg(Plex.serverName)
            case Plex.Connecting: return qsTr("Plex · connecting…")
            case Plex.Error: return qsTr("Plex · not connected")
            default: return qsTr("Plex · not signed in")
            }
        }
        return AppleMusic.authorized ? qsTr("Apple Music · %1").arg(AppleMusic.storefront.toUpperCase())
                                     : qsTr("Apple Music · not signed in")
    }
    readonly property color statusColor: {
        if (!Player.ready && Player.canLoad())
            return "#e0a030"
        if (Nav.plex)
            return Plex.ready ? "#3ccf6e" : Plex.status === Plex.Connecting ? "#e0a030"
                 : Plex.status === Plex.Error ? "#e05050" : Theme.textFaint
        return AppleMusic.authorized ? "#3ccf6e" : Theme.textFaint
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        spacing: 0

        // ---- brand ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 17
            Layout.rightMargin: 12
            spacing: 10
            // When collapsed, hovering the logo turns it into the expand button.
            AbstractButton {
                id: logo
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                enabled: root.collapsed
                readonly property bool showExpand: root.compact && hovered
                onClicked: root.collapsed = false
                ToolTip.visible: showExpand
                ToolTip.text: qsTr("Expand sidebar (Ctrl+B)")
                ToolTip.delay: 300
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: logo.showExpand ? (logo.pressed ? Theme.pressed : Theme.hover) : "transparent"
                }
                contentItem: Item {
                    Image {
                        anchors.fill: parent
                        visible: !logo.showExpand
                        source: "qrc:/qt/qml/Kanzi/resources/kanzi.svg"
                        sourceSize: Qt.size(30, 30)
                    }
                    Icon {
                        anchors.centerIn: parent
                        visible: logo.showExpand
                        name: "menu"
                        size: 19
                        color: Theme.textDim
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                visible: !root.compact
                text: "Kanzi"
                color: Theme.text
                font.pixelSize: 22
                font.weight: Font.Bold
                font.letterSpacing: -0.4
                elide: Text.ElideRight
            }
            IconButton {
                visible: !root.compact
                iconName: "menuOpen"
                iconSize: 17
                color: Theme.textDim
                tip: qsTr("Collapse sidebar (Ctrl+B)")
                onClicked: { root.collapsed = true; search.focus = false }
            }
        }

        // ---- search ----
        TextField {
            id: search
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 18
            Layout.bottomMargin: 10
            placeholderText: root.compact ? "" : qsTr("Search")
            ToolTip.visible: root.compact && hovered
            ToolTip.text: qsTr("Search")
            ToolTip.delay: 300
            leftPadding: 34
            color: Theme.text
            selectByMouse: true
            font.pixelSize: Theme.fontBody
            implicitHeight: 34

            background: Rectangle {
                radius: Theme.radiusSmall + 2
                color: Theme.hover
                border.width: search.activeFocus ? 2 : 0
                border.color: Theme.accent
                Icon {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    name: "search"
                    size: 17
                    color: Theme.textFaint
                }
            }

            onTextEdited: debounce.restart()
            onAccepted: { debounce.stop(); Nav.search(text) }
            Keys.onEscapePressed: { text = ""; focus = false }

            Timer {
                id: debounce
                interval: 320
                onTriggered: if (search.text.trim().length > 0) Nav.search(search.text)
            }
            Connections {
                target: Nav
                function onFocusSearch() { search.forceActiveFocus(); search.selectAll() }
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: nav.implicitHeight + 16
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: nav
                width: parent.width
                spacing: 1

                // Collapsed, the label keeps its space but shows a short divider instead.
                component SectionLabel: Text {
                    Layout.leftMargin: 24
                    Layout.topMargin: 18
                    Layout.bottomMargin: 6
                    // Inline components can't see `root`; the sidebar's width tells us instead.
                    readonly property bool compact: !!parent && parent.width < 100
                    color: compact ? "transparent" : Theme.textFaint
                    Rectangle {
                        visible: parent.compact && parent.visible
                        x: -2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20
                        height: 1
                        color: Theme.border
                    }
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.6
                    font.capitalization: Font.AllUppercase
                }
                component NavItem: SidebarItem {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    onClicked: Nav.show(key)
                }

                SectionLabel { text: Nav.plex ? qsTr("Plex") : qsTr("Apple Music") }
                NavItem { text: Nav.plex && !Nav.signedIn ? qsTr("Connect") : qsTr("Home"); iconName: "home"; key: Nav.signedIn ? "home" : "welcome"; visible: Nav.signedIn || Nav.plex }
                NavItem { text: qsTr("Browse"); iconName: "browse"; key: "browse"; visible: !Nav.plex }
                NavItem { text: qsTr("Search"); iconName: "search"; key: "search"; visible: !Nav.plex || Nav.signedIn }

                SectionLabel { text: qsTr("Library"); visible: Nav.signedIn }
                NavItem { text: qsTr("Recently Added"); iconName: "recent"; key: "recent"; visible: Nav.signedIn }
                NavItem { text: qsTr("Artists"); iconName: "artist"; key: "artists"; visible: Nav.signedIn }
                NavItem { text: qsTr("Albums"); iconName: "album"; key: "albums"; visible: Nav.signedIn }
                NavItem { text: qsTr("Songs"); iconName: "song"; key: "songs"; visible: Nav.signedIn }
                NavItem { text: qsTr("Playlists"); iconName: "playlist"; key: "playlists"; visible: Nav.signedIn }

                SectionLabel { text: qsTr("Playlists"); visible: root.playlists.length > 0 }
                Repeater {
                    model: root.playlists
                    SidebarItem {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        text: Nav.titleOf(modelData)
                        iconName: "playlist"
                        artwork: Nav.artworkOf(modelData)
                        onClicked: Nav.openItem(modelData)
                    }
                }
            }
        }

        // ---- account ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }
        GridLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            columns: root.compact ? 1 : 4
            rowSpacing: 4
            columnSpacing: 8

            // Status + source switcher: click to choose Apple Music or Plex.
            AbstractButton {
                id: sourceButton
                Layout.fillWidth: true
                implicitHeight: 30
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                onClicked: sourceMenu.popup(sourceButton, 0, -sourceMenu.implicitHeight - 4)
                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: Nav.plex && Plex.status === Plex.Error ? Plex.errorString
                            : root.compact ? root.statusText
                            : qsTr("Switch between Apple Music and Plex")

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: sourceButton.pressed ? Theme.pressed : sourceButton.hovered ? Theme.hover : "transparent"
                }
                contentItem: RowLayout {
                    spacing: 8
                    Rectangle {
                        Layout.leftMargin: root.compact ? 0 : 6
                        Layout.alignment: root.compact ? Qt.AlignHCenter : Qt.AlignLeft
                        implicitWidth: 8
                        implicitHeight: 8
                        radius: 4
                        color: root.statusColor
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !root.compact
                        text: root.statusText
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                    Icon {
                        Layout.rightMargin: 4
                        visible: !root.compact
                        name: "swap"
                        size: 15
                        color: Theme.textFaint
                    }
                }

                Menu {
                    id: sourceMenu
                    MenuItem {
                        text: qsTr("Apple Music")
                        checkable: true
                        checked: !Nav.plex
                        onTriggered: Player.source = Player.AppleMusicSource
                    }
                    MenuItem {
                        text: qsTr("Plex")
                        checkable: true
                        checked: Nav.plex
                        onTriggered: Player.source = Player.PlexSource
                    }
                }
            }
            IconButton {
                Layout.alignment: Qt.AlignHCenter
                iconName: "settings"
                iconSize: 17
                tip: qsTr("Settings")
                active: Nav.section === "settings"
                onClicked: Nav.show("settings")
            }
            IconButton {
                Layout.alignment: Qt.AlignHCenter
                visible: !Nav.plex
                iconName: AppleMusic.authorized ? "logout" : "artist"
                iconSize: 16
                tip: AppleMusic.authorized ? qsTr("Sign out") : qsTr("Sign in")
                enabled: Player.ready
                onClicked: AppleMusic.authorized ? AppleMusic.signOut() : Player.authorize()
            }
            IconButton {
                Layout.alignment: Qt.AlignHCenter
                visible: Nav.plex
                iconName: Plex.signedIn ? "logout" : "artist"
                iconSize: 16
                tip: Plex.signedIn ? qsTr("Sign out of Plex") : qsTr("Sign in to Plex")
                onClicked: Plex.signedIn ? Plex.signOut() : Plex.signIn()
            }
        }
    }
}
