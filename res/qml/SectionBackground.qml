import QtQuick 2.12
import "Theme"

// Drawn as vector rather than a 9-sliced section.svg. That file is only eight
// plain <rect>s -- no gradients, no filters -- so it carried no information a
// Rectangle cannot draw, while costing an image load and a rasterisation that
// was then stretched to the panel and upscaled again on a fractional-DPR
// screen. Geometry below is section.svg's, converted from its 13.229-unit
// viewBox to the 50px it declares (1 unit = 3.78px).
Rectangle {
    id: root

    color: "transparent"
    // 0.5px in the source; 1 logical px is the practical floor and stays crisp
    // because the border is drawn on the pixel grid rather than sampled.
    border.color: "black"
    border.width: 1

    // Inner highlight under the top edge.
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        color: Qt.rgba(1, 1, 1, 0.0627)
        height: 1
    }
    // Inner shade above the bottom edge.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: 1
        color: Qt.rgba(0, 0, 0, 0.1255)
        height: 1
    }
    // Faint inner highlights down each side.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        color: Qt.rgba(1, 1, 1, 0.0314)
        width: 1
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        color: Qt.rgba(1, 1, 1, 0.0314)
        width: 1
    }
}
