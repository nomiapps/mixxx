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
    // The length currently in force, or 0. A beatloop reports itself, so in
    // Loop mode it is read back from the engine rather than remembered here;
    // a one-shot is only known to the timer counting it out.
    readonly property real activeLength: root.looping ? (loopEnabledControl.value > 0 ? beatloopSizeControl.value : 0) : root.oneShotBeats
    // The sampler's BPM as it is playing, rate included. `bpm` is 0 until a
    // beatgrid exists, which is exactly when the length buttons cannot work.
    readonly property real beats: bpmControl.value > 0 ? bpmControl.value : root.visualBpm
    property var currentTrack: root.deckPlayer?.currentTrack ?? null
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    property int fxUnitCount: 4
    required property string group
    property int hotcueCount: 8
    readonly property var lengthChoices: [0.5, 1, 2, 4, 8, 16]
    readonly property bool loaded: trackLoadedControl.value > 0
    // The strip's Loop button (`repeat`), which is what decides whether a
    // length loops or cuts.
    readonly property bool looping: repeatControl.value > 0
    property bool minimized: false
    // Beats the running one-shot was asked for, 0 when none is counting.
    property real oneShotBeats: 0
    readonly property bool playing: playControl.value > 0
    property bool showFxAssignments: true
    property bool showHotcues: true
    property bool showLength: true
    property bool showRateControl: true
    readonly property real visualBpm: visualBpmControl.value

    signal fxAssignmentChanged(int unitNumber, bool enabled)

    // Something else stopped the sample -- Stop, Eject, the end of the file --
    // so the count that was running no longer means anything.
    onPlayingChanged: {
        if (!root.playing) {
            oneShotTimer.stop();
            root.oneShotBeats = 0;
        }
    }

    // Ends whichever kind of length is running and parks the sample on its cue,
    // the same place the Stop button leaves it.
    function stopLength() {
        oneShotTimer.stop();
        root.oneShotBeats = 0;
        if (loopEnabledControl.value > 0) {
            loopExitControl.value = 1;
            loopExitControl.value = 0;
        }
        cueGotoAndStopControl.value = 1;
        cueGotoAndStopControl.value = 0;
    }
    // Pressing the length that is already running cancels it, the way the play
    // button doubles as stop: a second press of 4 stops a 4-beat loop or cuts
    // a 4-beat one-shot short.
    function toggleLength(length) {
        if (root.activeLength === length) {
            root.stopLength();
            return;
        }
        oneShotTimer.stop();
        root.oneShotBeats = 0;
        // Whatever was looping belongs to the old length.
        if (loopEnabledControl.value > 0) {
            loopExitControl.value = 1;
            loopExitControl.value = 0;
        }
        cueGotoAndPlayControl.value = 1;
        cueGotoAndPlayControl.value = 0;
        if (root.looping) {
            beatloopSizeControl.value = length;
            // Set on the beat the sample has just started from, so the loop
            // covers its opening rather than wherever it had got to.
            beatloopActivateControl.value = 1;
            beatloopActivateControl.value = 0;
        } else {
            root.oneShotBeats = length;
            oneShotTimer.interval = Math.max(20, length * 60000 / root.beats);
            oneShotTimer.restart();
        }
    }

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
    // The length row is 20 high plus the column's 3 spacing; without the extra
    // the hotcue grid pays for it out of its own pads.
    implicitHeight: root.minimized ? 50 : (root.showLength ? 193 : 170)
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
        // Play doubles as Stop: pressing a sampler that is playing sends it back
        // to its cue instead of retriggering it. Nothing in the strip could stop
        // a sample before -- the only stop was an undiscoverable double-tap on
        // the strip, and a sample the sequencer fired had none at all. Holding
        // the right button still plays from the cue (cue_default), which is the
        // retrigger this gives up.
        //
        // Swapping `key` on a Skin.ControlButton looks like the small change and
        // is a trap: the press writes 1, `play` flips, and the release then
        // writes 0 to the OTHER control, leaving the first latched at 1. A push
        // button that never sees another rising edge is dead. So the button
        // holds whichever proxy it pressed and releases that one.
        Skin.Button {
            id: playButton

            property var heldControl: null

            function release() {
                if (!playButton.heldControl)
                    return;
                playButton.heldControl.value = 0;
                playButton.heldControl = null;
            }

            activeColor: Theme.samplerColor
            anchors.left: embedded.left
            anchors.top: embedded.top
            height: 40
            highlight: playLatchedControl.value > 0 || root.playing
            text: root.playing ? "Stop" : "Play"
            width: 40

            onCanceled: playButton.release()
            onPressed: {
                playButton.heldControl = root.playing ? cueGotoAndStopControl : cueGotoAndPlayControl;
                playButton.heldControl.value = 1;
            }
            onReleased: playButton.release()

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
                // Three seats and a marker need more room than a word does, so
                // it takes a wider share of the row than the buttons beside it.
                Layout.preferredWidth: 76
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
        // How much of the sample a hit plays. The Loop button beside it decides
        // what that means: with Loop on the length is a beatloop and the sample
        // cycles it until you stop it; with Loop off it is a one-shot cut that
        // plays that many beats and stops itself. Both restart from the cue, so
        // a length button is also a trigger.
        //
        // The one-shot is timed in QML from the sampler's own BPM rather than
        // by the engine -- Mixxx has no "play N beats and stop" control -- so
        // it lands within a buffer or two of the beat, which is right for a
        // stab and is not sample-accurate. The loop is the engine's own
        // beatloop and is exact.
        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            visible: root.showLength

            Skin.Button {
                Layout.preferredWidth: 26
                enabled: false
                implicitHeight: 20
                opacity: 0.7
                text: "LEN"
            }
            Repeater {
                model: root.lengthChoices

                Skin.Button {
                    required property real modelData

                    Layout.fillWidth: true
                    activeColor: Theme.samplerColor
                    // Without a beatgrid neither half of this works: a beatloop
                    // has no beats to measure and the one-shot has no BPM to
                    // count with.
                    enabled: root.loaded && root.beats > 0
                    highlight: root.activeLength === modelData
                    implicitHeight: 20
                    opacity: enabled ? 1 : 0.5
                    text: modelData < 1 ? "1/" + (1 / modelData) : modelData

                    onClicked: root.toggleLength(modelData)
                }
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
                // Clear of the pads above and the strip edge below: with the
                // slot inset removed the artwork runs the fader's full length,
                // so the gap has to come from the layout.
                Layout.bottomMargin: 4
                Layout.fillHeight: true
                Layout.topMargin: 4
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
        id: cueGotoAndPlayControl

        group: root.group
        key: "cue_gotoandplay"
    }
    // Back to the cue rather than a bare `play = 0`: it pairs with the play
    // above, so a stopped sample is parked where the next press starts it.
    Mixxx.ControlProxy {
        id: cueGotoAndStopControl

        group: root.group
        key: "cue_gotoandstop"
    }
    Mixxx.ControlProxy {
        id: repeatControl

        group: root.group
        key: "repeat"
    }
    Mixxx.ControlProxy {
        id: bpmControl

        group: root.group
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: beatloopSizeControl

        group: root.group
        key: "beatloop_size"
    }
    Mixxx.ControlProxy {
        id: beatloopActivateControl

        group: root.group
        key: "beatloop_activate"
    }
    Mixxx.ControlProxy {
        id: loopEnabledControl

        group: root.group
        key: "loop_enabled"
    }
    Mixxx.ControlProxy {
        id: loopExitControl

        group: root.group
        key: "loop_exit"
    }
    // A one-shot is only counted while the sample is actually running: stopping
    // it by any other means -- the Stop button, an eject, the sequencer firing
    // it again -- must not leave a timer behind to stop something else later.
    Timer {
        id: oneShotTimer

        onTriggered: {
            root.oneShotBeats = 0;
            if (root.playing) {
                cueGotoAndStopControl.value = 1;
                cueGotoAndStopControl.value = 0;
            }
        }
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
