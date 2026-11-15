import QtQuick
import QtQuick.Shapes
import Kanzi

Item {
    id: root
    property bool running: true
    property real size: 28
    property color color: Theme.textDim

    implicitWidth: size
    implicitHeight: size
    visible: running

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        RotationAnimator on rotation {
            from: 0; to: 360; duration: 900
            loops: Animation.Infinite
            running: root.running && root.visible
        }
        ShapePath {
            strokeColor: root.color
            strokeWidth: 3
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.size / 2; centerY: root.size / 2
                radiusX: root.size / 2 - 2; radiusY: root.size / 2 - 2
                startAngle: 0; sweepAngle: 270
            }
        }
    }
}
