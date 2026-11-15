import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

// A titled, horizontally scrolling row of MediaCards.
ColumnLayout {
    id: root

    property string title
    property var items: []
    property int cardWidth: 176

    spacing: 12
    visible: items && items.length > 0

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.gutter
        Layout.rightMargin: Theme.gutter - 8

        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.pixelSize: 21
            font.weight: Font.Bold
            elide: Text.ElideRight
        }
        IconButton {
            iconName: "back"
            iconSize: 18
            enabled: !list.atXBeginning
            onClicked: list.flick(1600, 0)
        }
        IconButton {
            iconName: "forward"
            iconSize: 18
            enabled: !list.atXEnd
            onClicked: list.flick(-1600, 0)
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.preferredHeight: root.cardWidth + 58
        orientation: ListView.Horizontal
        spacing: 20
        leftMargin: Theme.gutter
        rightMargin: Theme.gutter
        model: root.items
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 4000
        clip: true
        cacheBuffer: 600

        delegate: MediaCard {
            required property var modelData
            width: root.cardWidth
            item: modelData
        }

        WheelHandler {
            // Map vertical wheels to horizontal motion only when holding Shift, so
            // page scrolling over a shelf keeps working.
            acceptedModifiers: Qt.ShiftModifier
            onWheel: event => list.contentX = Math.max(-list.leftMargin,
                         Math.min(list.contentWidth - list.width + list.rightMargin, list.contentX - event.angleDelta.y))
        }
    }
}
