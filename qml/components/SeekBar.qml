import QtQuick
import QtQuick.Controls
import Kanzi

// Slim slider that thickens on hover. Reports the new value on release only.
Slider {
    id: root

    property real externalValue: 0
    property color trackColor: Theme.dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.12)
    property color fillColor: hovered || pressed ? Theme.accent : Theme.textDim
    signal committed(real value)

    from: 0
    to: 1
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    padding: 0
    implicitHeight: 16
    live: true

    Binding on value {
        when: !root.pressed
        value: root.externalValue
        restoreMode: Binding.RestoreNone
    }
    onPressedChanged: if (!pressed) committed(value)

    background: Item {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: root.hovered || root.pressed ? 6 : 4
        Behavior on height { NumberAnimation { duration: 100 } }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: root.trackColor
        }
        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: root.fillColor
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * root.availableWidth - width / 2
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 12
        height: 12
        radius: 6
        color: "white"
        border.color: Qt.rgba(0, 0, 0, 0.15)
        visible: root.hovered || root.pressed
    }
}
