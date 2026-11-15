import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

ItemDelegate {
    id: root

    property string iconName
    property string key
    property var artwork: null
    readonly property bool active: key !== "" && Nav.section === key

    implicitHeight: 34
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        radius: Theme.radiusSmall + 1
        color: root.active ? Theme.selected : root.pressed ? Theme.pressed : root.hovered ? Theme.hover : "transparent"
    }

    contentItem: RowLayout {
        spacing: 10
        Item {
            implicitWidth: 20
            implicitHeight: 20
            Icon {
                anchors.centerIn: parent
                visible: !root.artwork
                name: root.iconName
                size: 19
                color: root.active ? Theme.accent : Theme.textDim
            }
            Artwork {
                anchors.fill: parent
                visible: !!root.artwork
                artwork: root.artwork
                pixelSize: 40
                radius: 4
                fallbackIcon: "playlist"
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.text
            color: Theme.text
            elide: Text.ElideRight
            font.pixelSize: Theme.fontBody
            font.weight: root.active ? Font.DemiBold : Font.Normal
        }
    }
}
