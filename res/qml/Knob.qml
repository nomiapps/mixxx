import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import QtQuick.Window 2.12
import "Theme"

MixxxControls.Knob {
    id: root

    required property color color
    property url shadowSource: Theme.imgKnobShadow
    property url backgroundSource: Theme.imgKnob

    implicitWidth: background.width
    implicitHeight: implicitWidth
    arc: true
    arcRadius: width * 0.45
    arcOffsetY: width * 0.01
    arcColor: root.color
    arcWidth: 2
    angle: 116

    Image {
        id: shadow

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width * 7 / 6
        fillMode: Image.PreserveAspectFit
        source: root.shadowSource
        // rasterize the SVG at device resolution; without this it is drawn at
        // logical size and upscaled on HiDPI screens, which looks blurry
        sourceSize.width: width * Screen.devicePixelRatio
        sourceSize.height: height * Screen.devicePixelRatio
    }

    background: Image {
        id: background

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width
        source: root.backgroundSource
        sourceSize.width: width * Screen.devicePixelRatio
        sourceSize.height: height * Screen.devicePixelRatio
    }

    foreground: Item {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 2
            height: root.width / 5
            y: height
            color: root.color
        }
    }
}
