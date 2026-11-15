import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

// Slide-in "Up Next" panel.
Rectangle {
    id: root
    color: Theme.surface
    visible: x < (parent ? parent.width : 0)

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.border
    }

    // Swallow clicks so they don't reach the page underneath.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 1
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Text {
                Layout.fillWidth: true
                text: qsTr("Up Next")
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            IconButton {
                iconName: "close"
                iconSize: 18
                onClicked: Nav.queueOpen = false
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Player.queue
            spacing: 2
            leftMargin: 8
            rightMargin: 8
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var modelData
                required property int index
                readonly property bool current: index === Player.queuePosition
                readonly property bool played: index < Player.queuePosition

                width: list.width - 16
                implicitHeight: 54
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                opacity: played ? 0.45 : 1
                onClicked: Player.jumpTo(index)

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: row.current ? Theme.selected : row.hovered ? Theme.hover : "transparent"
                }

                contentItem: RowLayout {
                    spacing: 10
                    Artwork {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        artwork: row.modelData.artwork || null
                        pixelSize: 80
                        radius: 5
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.title
                            color: row.current ? Theme.accent : Theme.text
                            elide: Text.ElideRight
                            font.weight: row.current ? Font.DemiBold : Font.Normal
                        }
                        LinkText {
                            Layout.fillWidth: true
                            onClicked: Nav.openArtistOf(row.modelData)
                            text: row.modelData.artist
                            color: Theme.textDim
                            elide: Text.ElideRight
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                    Text {
                        text: Theme.formatTime(row.modelData.duration)
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }

            onVisibleChanged: if (visible && Player.queuePosition >= 0) positionViewAtIndex(Player.queuePosition, ListView.Beginning)

            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Your queue is empty")
                color: Theme.textFaint
            }
        }
    }
}
