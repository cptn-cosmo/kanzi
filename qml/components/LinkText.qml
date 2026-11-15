import QtQuick

// Text that behaves like a link: hand cursor and underline over the glyphs, `clicked` on tap.
Text {
    id: root

    property bool linkEnabled: true
    signal clicked()

    font.underline: linkEnabled && hover.hovered

    // Only the text itself is the link, not the empty space a layout may give it.
    Item {
        width: Math.min(root.contentWidth, root.width)
        height: root.height
        x: root.horizontalAlignment === Text.AlignHCenter ? (root.width - width) / 2
           : root.horizontalAlignment === Text.AlignRight ? root.width - width : 0
        HoverHandler { id: hover; enabled: root.linkEnabled; cursorShape: Qt.PointingHandCursor }
        TapHandler { enabled: root.linkEnabled; onTapped: root.clicked() }
    }
}
