import Mixxx 1.0 as Mixxx
import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    readonly property bool hasStems: stemCountControl.value > 0
    // Stems are drawn in kMaxSupportedStems lanes, not stem_count lanes, so a
    // 3-stem track still uses quarter-height lanes and the labels follow that.
    readonly property int laneCount: 4
    readonly property real laneHeight: wave.height / root.laneCount
    // The names only line up with anything when the stems are in their own
    // lanes, so an unsplit waveform gets no gutter and no labels.
    // [Waveform]/StemLabels bit 1 is the Edge surface (main window is bit 0). The layout
    // still opts in per surface; this is the global veto over it.
    readonly property bool labelsVisible: root.spec.stemLabels === true && root.splitStems && root.hasStems && (Mixxx.Config.waveformStemLabels & 2)
    readonly property real labelGutter: root.spec.stemLabelGutter ?? 64
    readonly property var player: root.groupResolved ? Mixxx.PlayerManager.getPlayer(root.groupResolved) : null
    readonly property bool splitStems: root.spec.splitStems === true
    // The stems model lives on the loaded track, so re-resolve it per track.
    readonly property var stemsModel: (root.hasStems && root.player && root.player.currentTrack) ? root.player.currentTrack.stemsModel : []
    required property var spec
    property var surface: null

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.groupResolved
        key: "stem_count"
    }
    // Stem names sit in a gutter beside the waveform, one per lane, so each
    // name is level with the stem it belongs to.
    Repeater {
        model: root.labelsVisible ? root.stemsModel : []

        Item {
            id: tag

            required property color color
            required property int index
            required property string label

            height: root.laneHeight
            width: root.labelGutter - 8
            x: 0
            y: wave.y + tag.index * root.laneHeight

            Text {
                id: name

                anchors.right: swatch.left
                anchors.rightMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.pureWhite
                elide: Text.ElideRight
                font.bold: true
                // Matches StemStrip, which renders these same names uppercase.
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.pixelSize: root.spec.stemLabelSize ?? 11
                horizontalAlignment: Text.AlignRight
                opacity: 0.85
                text: tag.label
                width: parent.width - swatch.width - 5
            }
            Rectangle {
                id: swatch

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: tag.color
                height: Math.min(18, root.laneHeight - 6)
                radius: 1
                width: 3
            }
        }
    }
    Skin.WaveformDisplay {
        id: wave

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: root.labelsVisible ? root.labelGutter : 0
        anchors.right: parent.right
        anchors.top: parent.top
        beatColor: root.spec.beatColor ?? "#a1a1a1a1"
        group: root.groupResolved
        splitStemTracks: root.splitStems
    }
    // Inner shadow frame: seats the waveform behind the surface plane.
    Rectangle {
        anchors.left: wave.left
        anchors.right: wave.right
        anchors.top: wave.top
        height: 6

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
        anchors.bottom: wave.bottom
        anchors.left: wave.left
        anchors.right: wave.right
        color: Theme.pureWhite
        height: 1
        opacity: 0.06
    }
    Rectangle {
        anchors.fill: wave
        border.color: "#000000"
        border.width: 1
        color: "transparent"
        opacity: 0.5
    }
}
