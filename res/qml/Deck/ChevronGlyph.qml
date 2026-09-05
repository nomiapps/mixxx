import QtQuick 2.12
import QtQuick.Shapes

Item {
    id: root

    property bool doubled: false
    property color fillColor: "#ffffff"
    property bool forward: true

    implicitHeight: root.doubled ? 14 : 10
    implicitWidth: root.doubled ? 20 : 12

    Shape {
        // Qt 6.6+ resolution-independent antialiasing; the older
        // geometry renderer stair-steps curves on some displays.
        preferredRendererType: Shape.CurveRenderer
        anchors.fill: parent
        antialiasing: true
        rotation: root.forward ? 0 : 180
        transformOrigin: Item.Center

        ShapePath {
            fillColor: root.fillColor
            fillRule: ShapePath.WindingFill
            startX: 0
            startY: root.doubled ? 1 : 0
            strokeColor: "transparent"

            PathLine {
                x: root.doubled ? 10 : root.width
                y: root.height / 2
            }
            PathLine {
                x: 0
                y: root.doubled ? root.height - 1 : root.height
            }
            PathLine {
                x: 0
                y: root.doubled ? 1 : 0
            }
        }
        ShapePath {
            fillColor: root.doubled ? root.fillColor : "transparent"
            fillRule: ShapePath.WindingFill
            startX: 10
            startY: 1
            strokeColor: "transparent"

            PathLine {
                x: 20
                y: root.height / 2
            }
            PathLine {
                x: 10
                y: root.height - 1
            }
            PathLine {
                x: 10
                y: 1
            }
        }
    }
}
