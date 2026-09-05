import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    required property var spec
    property var surface: null

    Skin.WaveformOverview {
        anchors.fill: parent
        group: root.groupResolved
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
