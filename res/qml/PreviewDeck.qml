import "." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import QtQuick.Shapes
import "Theme"

Rectangle {
    id: root

    color: 'transparent'

    Shape {
        // Qt 6.6+ resolution-independent antialiasing; the older
        // geometry renderer stair-steps curves on some displays.
        preferredRendererType: Shape.CurveRenderer
        anchors.fill: parent
        ShapePath {
            strokeColor: Theme.midGray
            strokeWidth: 1
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            startX: 0
            startY: 0
            PathLine { x: width; y: 0 }
            PathLine { x: width; y: height }
            PathLine { x: 0; y: height }
            PathLine { x: 0; y: 0 }
            PathLine { x: width; y: height }
            PathLine { x: 0; y: height }
            PathLine { x: width; y: 0 }
        }
    }

    Text {
        anchors.centerIn: parent
        color: 'white'
        text: "PreviewDeck placeholder"
    }
}
