import "." as Skin
import QtQuick 2.12
import "Theme"

// Track overview strip. Horizontal by default; a rect taller than it is wide
// (or spec.vertical: true) rotates it a quarter turn so the track runs bottom
// to top, matching a vertical EdgeElementWaveform beside it.
Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    required property var spec
    property var surface: null
    readonly property bool vertical: root.spec.vertical ?? (root.height > root.width)

    Skin.WaveformOverview {
        anchors.centerIn: parent
        group: root.groupResolved
        // Same trick as EdgeElementWaveform: swapped dimensions plus -90 degrees
        // makes the C++ overview, which only draws horizontally, read upward.
        height: root.vertical ? root.width : root.height
        rotation: root.vertical ? -90 : 0
        width: root.vertical ? root.height : root.width
    }

    // Inner shadow frame, matching EdgeElementWaveform.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 4

        gradient: Gradient {
            GradientStop {
                color: "#99000000"
                position: 0
            }
            GradientStop {
                color: "#00000000"
                position: 1
            }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.pureWhite
        height: 1
        opacity: 0.06
    }
    Rectangle {
        anchors.fill: parent
        border.color: "#000000"
        border.width: 1
        color: "transparent"
        opacity: 0.5
    }
}
