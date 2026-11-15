import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

AbstractButton {
    id: root

    property string iconName: ""
    property bool primary: true

    implicitHeight: 38
    implicitWidth: row.implicitWidth + 36
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    opacity: enabled ? 1 : 0.4

    background: Rectangle {
        radius: height / 2
        color: root.primary ? (root.hovered ? Theme.accentHover : Theme.accent)
                            : (root.pressed ? Theme.pressed : root.hovered ? Theme.selected : Theme.hover)
        scale: root.pressed ? 0.97 : 1
        Behavior on scale { NumberAnimation { duration: 90 } }
    }

    contentItem: Item {
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Icon {
                visible: root.iconName !== ""
                name: root.iconName
                size: 18
                color: root.primary ? Theme.accentText : Theme.accent
            }
            Text {
                text: root.text
                color: root.primary ? Theme.accentText : Theme.accent
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }
        }
    }
}
