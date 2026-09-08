pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts 1.12
import "Theme"

Rectangle {
    id: root

    // The row only builds a strip once its player exists, but [App],num_samplers
    // notifies before PlayerManager has finished adding them, so a strip can be
    // created a moment early. Tolerate the gap instead of throwing on every
    // evaluation until the player lands.
    property var currentTrack: root.deckPlayer?.currentTrack ?? null
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    property int fxUnitCount: 4
    required property string group
    property int hotcueCount: 8
    readonly property bool loaded: trackLoadedControl.value > 0
    property bool minimized: false
    readonly property bool playing: playControl.value > 0
    property bool showFxAssignments: true
    property bool showHotcues: true
    property bool showRateControl: true
    readonly property real visualBpm: visualBpmControl.value

    signal fxAssignmentChanged(int unitNumber, bool enabled)

    Drag.active: dragArea.drag.active
    Drag.dragType: Drag.Automatic
    Drag.mimeData: {
        let data = {
            "mixxx/player": root.group
        };
        const trackLocationUrl = root.currentTrack?.trackLocationUrl;
        if (trackLocationUrl)
            data["text/uri-list"] = trackLocationUrl;
        return data;
    }
    Drag.supportedActions: Qt.CopyAction
    color: {
        const trackColor = root.currentTrack?.color;
        if (!trackColor?.valid)
            return Theme.backgroundColor;
        return Qt.darker(trackColor, 2);
    }
    implicitHeight: root.minimized ? 50 : 170
    implicitWidth: 230

    Skin.SectionBackground {
        anchors.fill: parent
    }
    // Dragging a sampler is how you copy its track to a deck, and the strip
    // only has to REPORT that a drag is under way -- Drag.Automatic hands the
    // payload above to the system and draws its own cursor. Dragging `root`
    // moved the strip itself as well, and since these sit in a layout they do
    // not spring back: a sampler could be shoved out of the row and left there.
    // A stand-in carries the gesture instead. It is never shown and never
    // measured, so wherever the drag leaves it costs nothing.
    Item {
        id: dragProxy

        height: 0
        visible: false
        width: 0
    }
    MouseArea {
        id: dragArea

        anchors.fill: parent
        drag.target: dragProxy
    }
    Item {
        id: summary

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 50

        Skin.EmbeddedBackground {
            id: embedded

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 5
            anchors.right: vuMeter.left
            anchors.top: parent.top
        }
        Skin.ControlMiniKnob {
            id: gainKnob

            anchors.margins: 5
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.samplerColor
            group: root.group
            height: 40
            key: "pregain"
            width: 40
        }
        Skin.ControlButton {
            id: playButton

            activeColor: Theme.samplerColor
            anchors.left: embedded.left
            anchors.top: embedded.top
            group: root.group
            height: 40
            highlight: playLatchedControl.value > 0 || root.playing
            key: "cue_gotoandplay"
            text: "Play"
            width: 40

            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent

                onCanceled: cueDefaultControl.value = 0
                onPressed: cueDefaultControl.value = 1
                onReleased: cueDefaultControl.value = 0
            }
        }
        Text {
            id: label

            anchors.left: playButton.right
            anchors.leftMargin: 5
            anchors.right: bpmLabel.left
            anchors.rightMargin: 5
            anchors.top: embedded.top
            color: Theme.deckTextColor
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
            text: root.currentTrack?.title ?? ""
        }
        Text {
            id: bpmLabel

            anchors.right: embedded.right
            anchors.top: embedded.top
            color: Theme.deckTextColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.buttonFontPixelSize
            text: root.loaded && root.visualBpm > 0 ? root.visualBpm.toFixed(1) : ""
        }
        Skin.WaveformOverview {
            anchors.bottom: embedded.bottom
            anchors.left: playButton.right
            anchors.leftMargin: 5
            anchors.right: embedded.right
            anchors.top: label.bottom
            anchors.topMargin: 2
            group: root.group
        }
        Skin.VuMeter {
            id: vuMeter

            anchors.bottom: parent.bottom
            anchors.margins: 5
            anchors.right: gainKnob.left
            anchors.top: parent.top
            group: root.group
            key: "vu_meter"
            width: 4
        }
    }
    // The button row spans the whole strip and the fader sits below it, beside
    // the pads. It used to be the other way round -- one tall column of buttons
    // and pads with a full-height fader alongside -- which left the fader as
    // tall as the strip while Sync, Loop, the crossfader slider and Eject were
    // squeezed into the width the fader did not take.
    ColumnLayout {
        id: expandedControls

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: summary.bottom
        spacing: 3
        visible: !root.minimized

        RowLayout {
            Layout.fillWidth: true
            spacing: 2

            Skin.ControlButton {
                Layout.fillWidth: true
                group: root.group
                key: "sync_enabled"
                text: "Sync"
                toggleable: true
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                group: root.group
                key: "keylock"
                text: "Key"
                toggleable: true
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                group: root.group
                key: "repeat"
                text: "Loop"
                toggleable: true
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                group: root.group
                key: "pfl"
                text: "PFL"
                toggleable: true
            }
            Skin.OrientationToggleButton {
                Layout.fillWidth: true
                color: Theme.crossfaderOrientationColor
                group: root.group
                key: "orientation"
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                group: root.group
                key: "eject"
                text: "Eject"
            }
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 3

            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 3

                GridLayout {
                    Layout.fillWidth: true
                    columnSpacing: 2
                    columns: 4
                    rowSpacing: 2
                    visible: root.showHotcues && root.hotcueCount > 0

                    Repeater {
                        model: Math.min(8, Math.max(0, root.hotcueCount))

                        Skin.HotcueButton {
                            required property int index

                            Layout.fillWidth: true
                            group: root.group
                            hotcueNumber: index + 1
                            implicitHeight: 22
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    visible: root.showFxAssignments && root.fxUnitCount > 0

                    Repeater {
                        model: Math.max(0, root.fxUnitCount)

                        Skin.ControlButton {
                            id: fxButton

                            required property int index

                            Layout.fillWidth: true
                            activeColor: Theme.effectUnitColor
                            group: "[EffectRack1_EffectUnit" + (index + 1) + "]"
                            implicitHeight: 22
                            key: "group_" + root.group + "_enable"
                            text: "FX" + (index + 1)
                            toggleable: true

                            onHighlightChanged: root.fxAssignmentChanged(index + 1, highlight)
                        }
                    }
                }
            }
            // The speed fader runs vertically, like the deck's, so it reads as a
            // turntable pitch fader: with the default rate direction inverted, up
            // is slower and + sits at the bottom. Laid out horizontally it also
            // took its implicit size from the slot artwork, which is a tall
            // vertical image, so it reported a large implicit height and swallowed
            // the strip. A fixed narrow width across the travel axis sizes it to
            // the space instead, and the cap scales itself down to match.
            Skin.ControlFader {
                Layout.fillHeight: true
                // A preferred width is only a preference: a RowLayout squeezes it
                // when the rest of the strip wants more room, and these strips are
                // narrow. Squeezed to a couple of pixels the slot artwork had
                // nowhere to draw, so the fader showed as a bare line with a cap
                // on it. A minimum keeps the groove.
                Layout.minimumWidth: 22
                Layout.preferredWidth: 22
                bar.color: Theme.bpmSliderBarColor
                // Skin.Fader insets its slot by 10 px a side, which is meant for
                // a wide mixer fader; on a 22 px one it left 2 px of groove and
                // the artwork read as a bare line. The deck tempo slider, which
                // this is the sampler equivalent of, sets the same inset to zero.
                bar.margin: 0
                bar.start: 0.5
                bg: Theme.imgBpmSliderBackground
                group: root.group
                key: "rate"
                visible: root.showRateControl
            }
        }
    }
    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: visualBpmControl

        group: root.group
        key: "visual_bpm"
    }
    Mixxx.ControlProxy {
        id: playControl

        group: root.group
        key: "play"
    }
    Mixxx.ControlProxy {
        id: playLatchedControl

        group: root.group
        key: "play_latched"
    }
    Mixxx.ControlProxy {
        id: cueDefaultControl

        group: root.group
        key: "cue_default"
    }
    Mixxx.ControlProxy {
        id: ejectControl

        group: root.group
        key: "eject"
    }
    TapHandler {
        onDoubleTapped: {
            if (root.playing)
                playControl.value = 0;
            else
                ejectControl.trigger();
        }
    }
    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
    }
}
