import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

// Square artwork tile for albums, playlists, stations and artists.
Item {
    id: root

    property var item: null
    readonly property string type: Nav.typeOf(item).replace("library-", "")
    readonly property bool isArtist: type === "artists"
    readonly property bool playable: Nav.playKind(item) !== ""

    implicitWidth: 180
    implicitHeight: art.height + labels.implicitHeight + 10

    Artwork {
        id: art
        width: root.width
        height: width
        artwork: Nav.artworkOf(root.item)
        pixelSize: Math.max(200, root.width)
        circle: root.isArtist
        radius: Theme.radius
        fallbackIcon: root.isArtist ? "artist" : root.type === "playlists" ? "playlist" : root.type === "stations" ? "radio" : "album"

        scale: hover.hovered ? 1.015 : 1
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            radius: art.effectiveRadius
            color: "black"
            opacity: hover.hovered ? 0.22 : 0
            Behavior on opacity { NumberAnimation { duration: 140 } }
        }

        IconButton {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 10
            visible: root.playable && hover.hovered
            iconName: "play"
            iconSize: 18
            filled: true
            onClicked: Nav.playItem(root.item)
        }

        IconButton {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            visible: root.playable && hover.hovered
            iconName: "more"
            iconSize: 18
            color: "white"
            background: Rectangle { radius: width / 2; color: Qt.rgba(0, 0, 0, 0.45) }
            onClicked: menu.popup()
        }
    }

    ColumnLayout {
        id: labels
        anchors.top: art.bottom
        anchors.topMargin: 8
        width: root.width
        spacing: 2

        Text {
            Layout.fillWidth: true
            text: Nav.titleOf(root.item)
            color: Theme.text
            elide: Text.ElideRight
            font.pixelSize: Theme.fontBody
            font.weight: Font.Medium
            horizontalAlignment: root.isArtist ? Text.AlignHCenter : Text.AlignLeft
        }
        LinkText {
            Layout.fillWidth: true
            visible: !root.isArtist && text !== ""
            // Albums and songs link to their artist; curators and labels don't.
            linkEnabled: (root.type === "albums" || root.type === "songs") && !!Nav.attrs(root.item).artistName
            onClicked: Nav.openArtistOf(root.item)
            text: Nav.subtitleOf(root.item)
            color: Theme.textDim
            elide: Text.ElideRight
            font.pixelSize: Theme.fontSmall + 1
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: art
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        z: -1
        onClicked: mouseEvent => {
            if (mouseEvent.button === Qt.RightButton && (root.playable || Nav.canShare(root.item)))
                menu.popup()
            else if (mouseEvent.button === Qt.LeftButton)
                Nav.canOpen(root.item) ? Nav.openItem(root.item) : Nav.playItem(root.item)
        }
    }

    HoverHandler {
        id: hover
    }

    Menu {
        id: menu
        MenuItem { text: qsTr("Play"); onTriggered: Nav.playItem(root.item) }
        MenuItem { text: qsTr("Shuffle"); onTriggered: Nav.playItem(root.item, true) }
        MenuSeparator {}
        MenuItem { text: qsTr("Play Next"); onTriggered: Nav.queueItem(root.item, true) }
        MenuItem { text: qsTr("Play Later"); onTriggered: Nav.queueItem(root.item, false) }
        MenuSeparator { visible: shareItem.visible }
        MenuItem {
            id: shareItem
            text: qsTr("Copy Apple Music Link")
            visible: Nav.canShare(root.item)
            height: visible ? implicitHeight : 0
            onTriggered: Nav.share(root.item)
        }
    }
}
