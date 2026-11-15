import QtQuick
import QtQuick.Layouts
import Kanzi

// Page title row with a back button when there is history to go back to.
ColumnLayout {
    id: root

    property string title
    property string subtitle
    default property alias actions: actionRow.data

    spacing: 4

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.gutter - (back.visible ? 10 : 0)
        Layout.rightMargin: Theme.gutter
        Layout.topMargin: 28
        spacing: 6

        IconButton {
            id: back
            visible: Nav.stack && Nav.stack.depth > 1
            iconName: "back"
            iconSize: 22
            onClicked: Nav.back()
        }

        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.weight: Font.Bold
            elide: Text.ElideRight
        }

        RowLayout {
            id: actionRow
            spacing: 8
        }
    }

    Text {
        Layout.leftMargin: Theme.gutter
        visible: root.subtitle !== ""
        text: root.subtitle
        color: Theme.textDim
        font.pixelSize: Theme.fontBody
    }
}
