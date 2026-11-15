import QtQuick
import QtQuick.Effects
import Kanzi

// Rounded (or circular) artwork with a tinted placeholder while it loads.
Item {
    id: root

    property var artwork: null       // Apple artwork object or URL template
    property int pixelSize: 300      // requested resolution in logical pixels
    property real radius: Theme.radius
    property bool circle: false
    property string fallbackIcon: "song"

    readonly property real effectiveRadius: circle ? Math.min(width, height) / 2 : radius
    readonly property bool loaded: img.status === Image.Ready

    Rectangle {
        anchors.fill: parent
        radius: root.effectiveRadius
        color: Theme.artColor(root.artwork && root.artwork.bgColor ? root.artwork : null)
        visible: !root.loaded

        Icon {
            anchors.centerIn: parent
            name: root.fallbackIcon
            size: Math.max(16, Math.min(root.width, root.height) * 0.32)
            color: Theme.textFaint
            visible: !root.artwork || img.status === Image.Error
        }
    }

    Image {
        id: img
        anchors.fill: parent
        visible: false
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(px, px)
        readonly property int px: Math.round(root.pixelSize * Screen.devicePixelRatio)
        source: root.artwork ? AppleMusic.artworkUrl(root.artwork, px) : ""
    }

    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        layer.smooth: true
        Rectangle {
            anchors.fill: parent
            radius: root.effectiveRadius
            color: "black"
        }
    }

    MultiEffect {
        anchors.fill: parent
        source: img
        visible: root.loaded
        opacity: root.loaded ? 1 : 0
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
        Behavior on opacity { NumberAnimation { duration: 220 } }
    }
}
