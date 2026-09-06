import Mixxx 1.0 as Mixxx
import "." as Skin
import QtQuick 2.12
import "Theme"

// A deck waveform on the Edge canvas. Horizontal by default; a rect taller
// than it is wide (or spec.vertical: true) turns it into a vertical waveform
// with the future at the top and the play position marker running left to
// right, the way vertical waveforms read on a CDJ screen.
//
// The C++ display only knows how to draw horizontally, so the vertical mode is
// the same item rotated a quarter turn: an item sized (height x width), spun
// -90 degrees about its centre, comes out (width x height). Everything inside
// -- renderers, marks, the scratch MouseArea -- keeps working in its own local
// frame; only the stem name gutter has to know which way is up.
Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    readonly property bool hasStems: stemCountControl.value > 0
    // Beside the waveform when horizontal, above it when vertical: the default
    // is sized for a name at the side, so a vertical layout usually sets its own.
    readonly property real labelGutter: root.spec.stemLabelGutter ?? (root.vertical ? 22 : 64)
    // The names only line up with anything when the stems are in their own
    // lanes, so an unsplit waveform gets no gutter and no labels.
    // [Waveform]/StemLabels bit 1 is the Edge surface (main window is bit 0). The layout
    // still opts in per surface; this is the global veto over it.
    readonly property bool labelsVisible: root.spec.stemLabels === true && root.splitStems && root.hasStems && (Mixxx.Config.waveformStemLabels & 2)
    // Lanes stack along the waveform's breadth: down the height when horizontal,
    // across the width when vertical (the rotation turns the top lane into the
    // leftmost one).
    readonly property real laneBreadth: (root.vertical ? waveHost.width : waveHost.height) / root.laneCount
    // Stems are drawn in kMaxSupportedStems lanes, not stem_count lanes, so a
    // 3-stem track still uses quarter-height lanes and the labels follow that.
    readonly property int laneCount: 4
    readonly property var player: root.groupResolved ? Mixxx.PlayerManager.getPlayer(root.groupResolved) : null
    required property var spec
    readonly property bool splitStems: root.spec.splitStems === true
    // The stems model lives on the loaded track, so re-resolve it per track.
    readonly property var stemsModel: (root.hasStems && root.player && root.player.currentTrack) ? root.player.currentTrack.stemsModel : []
    property var surface: null
    readonly property bool vertical: root.spec.vertical ?? (root.height > root.width)

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.groupResolved
        key: "stem_count"
    }
    // Stem names sit in a gutter beside (horizontal) or above (vertical) the
    // waveform, one per lane, so each name is level with the stem it belongs to.
    Repeater {
        model: root.labelsVisible ? root.stemsModel : []

        Item {
            id: tag

            required property color color
            required property int index
            required property string label

            height: root.vertical ? root.labelGutter - 4 : root.laneBreadth
            width: root.vertical ? root.laneBreadth : root.labelGutter - 8
            x: root.vertical ? waveHost.x + tag.index * root.laneBreadth : 0
            y: root.vertical ? 0 : waveHost.y + tag.index * root.laneBreadth

            Text {
                id: name

                anchors.bottom: root.vertical ? swatch.top : undefined
                anchors.bottomMargin: root.vertical ? 2 : 0
                anchors.horizontalCenter: root.vertical ? parent.horizontalCenter : undefined
                anchors.right: root.vertical ? undefined : swatch.left
                anchors.rightMargin: root.vertical ? 0 : 5
                anchors.verticalCenter: root.vertical ? undefined : parent.verticalCenter
                color: Theme.pureWhite
                elide: Text.ElideRight
                font.bold: true
                // Matches StemStrip, which renders these same names uppercase.
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.pixelSize: root.spec.stemLabelSize ?? 11
                horizontalAlignment: root.vertical ? Text.AlignHCenter : Text.AlignRight
                opacity: 0.85
                text: tag.label
                width: root.vertical ? parent.width - 4 : parent.width - swatch.width - 5
            }
            Rectangle {
                id: swatch

                anchors.bottom: root.vertical ? parent.bottom : undefined
                anchors.horizontalCenter: root.vertical ? parent.horizontalCenter : undefined
                anchors.right: root.vertical ? undefined : parent.right
                anchors.verticalCenter: root.vertical ? undefined : parent.verticalCenter
                color: tag.color
                height: root.vertical ? 3 : Math.min(18, root.laneBreadth - 6)
                radius: 1
                width: root.vertical ? Math.min(18, root.laneBreadth - 6) : 3
            }
        }
    }
    // The waveform's footprint on the canvas. The display itself is a child so
    // that the vertical rotation is confined to it and the frame below stays
    // axis-aligned.
    Item {
        id: waveHost

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: root.labelsVisible && !root.vertical ? root.labelGutter : 0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: root.labelsVisible && root.vertical ? root.labelGutter : 0

        Skin.WaveformDisplay {
            id: wave

            anchors.centerIn: parent
            beatColor: root.spec.beatColor ?? "#a1a1a1a1"
            group: root.groupResolved
            // Swapped dimensions plus a quarter turn: the item is laid out as a
            // wide horizontal waveform and spun so its future end points up.
            height: root.vertical ? waveHost.width : waveHost.height
            rotation: root.vertical ? -90 : 0
            splitStemTracks: root.splitStems
            width: root.vertical ? waveHost.height : waveHost.width
        }
    }
    // Inner shadow frame: seats the waveform behind the surface plane.
    Rectangle {
        anchors.left: waveHost.left
        anchors.right: waveHost.right
        anchors.top: waveHost.top
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
        anchors.bottom: waveHost.bottom
        anchors.left: waveHost.left
        anchors.right: waveHost.right
        color: Theme.pureWhite
        height: 1
        opacity: 0.06
    }
    Rectangle {
        anchors.fill: waveHost
        border.color: "#000000"
        border.width: 1
        color: "transparent"
        opacity: 0.5
    }
}
