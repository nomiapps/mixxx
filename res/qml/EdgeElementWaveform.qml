import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")

    Skin.WaveformDisplay {
        anchors.fill: parent
        group: root.groupResolved
        splitStemTracks: root.spec.splitStems === true
        beatColor: root.spec.beatColor ?? "#a1a1a1a1"
    }

    // Inner shadow frame: seats the waveform behind the surface plane.
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 6
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#99000000"
            }

            GradientStop {
                position: 1
                color: "#00000000"
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.pureWhite
        opacity: 0.06
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "#000000"
        border.width: 1
        opacity: 0.5
    }
}
