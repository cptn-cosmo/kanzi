import QtQuick
import QtQuick.Controls
import Kanzi

AbstractButton {
    id: root

    property string iconName: ""
    property real iconSize: 20
    property color color: Theme.text
    property bool active: false
    property bool filled: false // solid accent circle, used for the main play button
    property string tip: ""

    implicitWidth: Math.round(iconSize * 1.9)
    implicitHeight: implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    opacity: enabled ? 1 : 0.35

    background: Rectangle {
        radius: width / 2
        color: root.filled ? (root.hovered ? Theme.accentHover : Theme.accent)
                           : root.pressed ? Theme.pressed : root.hovered ? Theme.hover : "transparent"
        scale: root.pressed ? 0.94 : 1
        Behavior on scale { NumberAnimation { duration: 90 } }
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: root.iconName
            size: root.iconSize
            color: root.filled ? Theme.accentText : root.active ? Theme.accent : root.color
        }
        Rectangle {
            visible: root.active && !root.filled
            width: 4; height: 4; radius: 2
            color: Theme.accent
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -1
        }
    }

    ToolTip.visible: tip !== "" && hovered
    ToolTip.text: tip
    ToolTip.delay: 600
}
