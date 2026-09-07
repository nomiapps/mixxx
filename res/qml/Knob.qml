import Edge.Controls 1.0 as EdgeControls
import QtQuick 2.12
import QtQuick.Window 2.12
import "Theme"

EdgeControls.Knob {
    id: root

    property url backgroundSource: Theme.imgKnob
    required property color color
    property url shadowSource: Theme.imgKnobShadow
    property bool showDefaultBackground: true
    property bool showDefaultForeground: true

    angle: 116
    arc: true
    arcColor: root.color
    arcOffsetY: width * 0.01
    arcRadius: width * 0.45
    arcWidth: 2
    implicitHeight: implicitWidth
    implicitWidth: background.width

    // RasterImage, not Image: it rasterises at device resolution (a plain SVG
    // is drawn at logical size and upscaled, which looks blurry) but buckets the
    // raster size, so a window resize does not re-decode the SVG on every pixel.
    background: RasterImage {
        id: background

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: width
        source: root.backgroundSource
        visible: root.showDefaultBackground
    }
    foreground: Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: width
        visible: root.showDefaultForeground

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.color
            height: root.width / 5
            width: 2
            y: height
        }
    }

    RasterImage {
        id: shadow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        fillMode: Image.PreserveAspectFit
        height: width * 7 / 6
        source: root.shadowSource
        visible: root.showDefaultBackground
    }
}
