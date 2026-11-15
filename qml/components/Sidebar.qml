import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Rectangle {
    id: root
    color: Theme.sidebar

    property var playlists: []

    function loadPlaylists() {
        if (!AppleMusic.authorized) {
            playlists = []
            return
        }
        AppleMusic.get("/v1/me/library/playlists", { limit: 100 }, (res, err) => {
            if (!err && res.data)
                root.playlists = res.data
        })
    }

    Connections {
        target: AppleMusic
        function onUserTokenChanged() { root.loadPlaylists() }
        function onStatusChanged() { if (AppleMusic.status === AppleMusic.Ready) root.loadPlaylists() }
    }
    Component.onCompleted: if (AppleMusic.status === AppleMusic.Ready) loadPlaylists()

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        spacing: 0

        // ---- brand ----
        RowLayout {
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 10
            Image {
                source: "qrc:/qt/qml/Kanzi/resources/kanzi.svg"
                sourceSize: Qt.size(30, 30)
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
            }
            Text {
                text: "Kanzi"
                color: Theme.text
                font.pixelSize: 22
                font.weight: Font.Bold
                font.letterSpacing: -0.4
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
            placeholderText: qsTr("Search")
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

                component SectionLabel: Text {
                    Layout.leftMargin: 24
                    Layout.topMargin: 18
                    Layout.bottomMargin: 6
                    color: Theme.textFaint
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

                SectionLabel { text: qsTr("Apple Music") }
                NavItem { text: qsTr("Home"); iconName: "home"; key: "home"; visible: AppleMusic.authorized }
                NavItem { text: qsTr("Browse"); iconName: "browse"; key: "browse" }
                NavItem { text: qsTr("Search"); iconName: "search"; key: "search" }

                SectionLabel { text: qsTr("Library"); visible: AppleMusic.authorized }
                NavItem { text: qsTr("Recently Added"); iconName: "recent"; key: "recent"; visible: AppleMusic.authorized }
                NavItem { text: qsTr("Artists"); iconName: "artist"; key: "artists"; visible: AppleMusic.authorized }
                NavItem { text: qsTr("Albums"); iconName: "album"; key: "albums"; visible: AppleMusic.authorized }
                NavItem { text: qsTr("Songs"); iconName: "song"; key: "songs"; visible: AppleMusic.authorized }
                NavItem { text: qsTr("Playlists"); iconName: "playlist"; key: "playlists"; visible: AppleMusic.authorized }

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
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 8

            Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                radius: 4
                color: !Player.ready ? "#e0a030" : AppleMusic.authorized ? "#3ccf6e" : Theme.textFaint
                ToolTip.visible: statusHover.hovered
                ToolTip.text: !Player.ready ? qsTr("Starting player…") : AppleMusic.authorized ? qsTr("Signed in") : qsTr("Not signed in")
                HoverHandler { id: statusHover }
            }
            Text {
                Layout.fillWidth: true
                text: AppleMusic.authorized ? qsTr("Apple Music · %1").arg(AppleMusic.storefront.toUpperCase())
                                            : qsTr("Not signed in")
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
            IconButton {
                iconName: "settings"
                iconSize: 17
                tip: qsTr("Settings")
                active: Nav.section === "settings"
                onClicked: Nav.show("settings")
            }
            IconButton {
                iconName: AppleMusic.authorized ? "logout" : "artist"
                iconSize: 16
                tip: AppleMusic.authorized ? qsTr("Sign out") : qsTr("Sign in")
                enabled: Player.ready
                onClicked: AppleMusic.authorized ? AppleMusic.signOut() : Player.authorize()
            }
        }
    }
}
