pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// A 16-step sequencer over the [SequencerN] engine component: a pitched lane
// that plays the synth and drum lanes that each fire a sampler, locked to the
// sync leader's beat. spec fields:
//   group         the sequencer group (default "[Sequencer1]")
//   steps         steps shown (default 16; the engine pattern is 16)
//   samplerLanes  drum lanes (default 4)
//   samplerCount  how many samplers a lane header cycles through (default 8)
// Every cell is a control, so a controller mapping can edit the same pattern.
Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "[Sequencer1]") : (spec.group ?? "[Sequencer1]")
    readonly property var noteNames: ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    readonly property int samplerCount: Math.max(1, spec.samplerCount ?? 8)
    readonly property int samplerLanes: Math.max(0, Math.min(4, spec.samplerLanes ?? 4))
    property int selectedStep: 0
    required property var spec
    readonly property int steps: Math.max(1, Math.min(16, spec.steps ?? 16))
    property var surface: null

    function noteName(value) {
        const note = Math.max(0, Math.min(127, Math.round(value)));
        return noteNames[note % 12] + (Math.floor(note / 12) - 1);
    }

    Mixxx.ControlProxy {
        id: runControl

        group: root.groupResolved
        key: "run"
    }
    Mixxx.ControlProxy {
        id: currentStepControl

        group: root.groupResolved
        key: "current_step"
    }
    Mixxx.ControlProxy {
        id: lengthControl

        group: root.groupResolved
        key: "length"
    }
    Mixxx.ControlProxy {
        id: clockBpm

        group: "[InternalClock]"
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: selectedNote

        group: root.groupResolved
        key: "synth_step_" + (root.selectedStep + 1) + "_note"
    }
    Row {
        id: transport

        readonly property real buttonHeight: height * 0.8

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.height * 0.16
        spacing: Math.max(6, root.width * 0.008)

        Skin.ControlButton {
            activeColor: Theme.green
            anchors.verticalCenter: parent.verticalCenter
            group: root.groupResolved
            height: transport.buttonHeight
            key: "run"
            text: "RUN"
            toggleable: true
            width: transport.buttonHeight * 2
        }
        Skin.EdgePadButton {
            activeColor: Theme.blue
            anchors.verticalCenter: parent.verticalCenter
            height: transport.buttonHeight
            padGroup: root.groupResolved
            padKey: "restart"
            text: "RESTART"
            width: transport.buttonHeight * 2.4
        }
        Item {
            height: transport.height
            width: transport.buttonHeight * 1.2

            Text {
                id: swingLabel

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                color: Theme.deckTextColor
                font.pixelSize: 10
                text: "SWING"
            }
            Skin.ControlKnob {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: swingLabel.bottom
                anchors.topMargin: 2
                color: Theme.amber
                group: root.groupResolved
                height: width
                key: "swing"
                width: Math.min(parent.width, transport.height * 0.7)
            }
        }
        Skin.Button {
            anchors.verticalCenter: parent.verticalCenter
            height: transport.buttonHeight
            text: "-"
            width: transport.buttonHeight

            onClicked: lengthControl.value = Math.max(1, Math.round(lengthControl.value) - 1)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.deckTextColor
            font.pixelSize: 14
            text: "LEN " + Math.round(lengthControl.value)
        }
        Skin.Button {
            anchors.verticalCenter: parent.verticalCenter
            height: transport.buttonHeight
            text: "+"
            width: transport.buttonHeight

            onClicked: lengthControl.value = Math.min(root.steps, Math.round(lengthControl.value) + 1)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.deckTextColor
            font.pixelSize: 14
            text: "STEP " + (currentStepControl.value >= 0 ? (Math.round(currentStepControl.value) + 1) : "-")
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.deckTextColor
            font.pixelSize: 14
            text: clockBpm.value.toFixed(1) + " BPM"
        }
    }
    Item {
        id: grid

        readonly property real cellHeight: (height - gap * root.samplerLanes) / (root.samplerLanes + 1)
        readonly property real cellWidth: (width - labelWidth - gap * root.steps) / root.steps
        readonly property real gap: Math.max(3, height * 0.03)
        readonly property real labelWidth: width * 0.06

        anchors.bottom: detail.top
        anchors.bottomMargin: gap
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: transport.bottom
        anchors.topMargin: gap

        Column {
            anchors.fill: parent
            spacing: grid.gap

            Row {
                height: grid.cellHeight
                spacing: grid.gap

                Text {
                    color: Theme.blue
                    font.bold: true
                    font.pixelSize: 12
                    height: grid.cellHeight
                    text: "SYNTH"
                    verticalAlignment: Text.AlignVCenter
                    width: grid.labelWidth
                }
                Repeater {
                    model: root.steps

                    SynthCell {
                        height: grid.cellHeight
                        width: grid.cellWidth
                    }
                }
            }
            Repeater {
                model: root.samplerLanes

                Row {
                    id: laneRow

                    required property int index

                    height: grid.cellHeight
                    spacing: grid.gap

                    Mixxx.ControlProxy {
                        id: targetControl

                        group: root.groupResolved
                        key: "sampler_" + (laneRow.index + 1) + "_target"
                    }
                    Skin.Button {
                        activeColor: Theme.purple
                        fontPixelSize: 12
                        height: grid.cellHeight
                        highlight: true
                        text: "S" + Math.round(targetControl.value)
                        width: grid.labelWidth

                        onClicked: targetControl.value = (Math.round(targetControl.value) % root.samplerCount) + 1
                    }
                    Repeater {
                        model: root.steps

                        Skin.ControlButton {
                            required property int index

                            activeColor: Theme.purple
                            group: root.groupResolved
                            height: grid.cellHeight
                            key: "sampler_" + (laneRow.index + 1) + "_step_" + (index + 1) + "_enabled"
                            normalColor: Math.floor(index / 4) % 2 ? Theme.darkGray4 : Theme.darkGray2
                            toggleable: true
                            width: grid.cellWidth
                        }
                    }
                }
            }
        }
        // The playhead: a wash over the column the engine last fired.
        Rectangle {
            color: Theme.pressedWashColor
            height: parent.height
            radius: 3
            visible: runControl.value > 0 && currentStepControl.value >= 0
            width: grid.cellWidth
            x: grid.labelWidth + grid.gap + Math.max(0, Math.round(currentStepControl.value)) * (grid.cellWidth + grid.gap)
            y: 0
        }
    }
    // The selected synth step's velocity, gate and note, for hands that would
    // rather turn a knob than drag a cell.
    Row {
        id: detail

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.height * 0.2
        spacing: Math.max(6, root.width * 0.008)

        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.deckTextColor
            font.pixelSize: 14
            text: "STEP " + (root.selectedStep + 1)
        }
        Repeater {
            model: [
                {
                    "key": "velocity",
                    "label": "VEL"
                },
                {
                    "key": "gate",
                    "label": "GATE"
                }
            ]

            Item {
                required property var modelData

                height: detail.height
                width: detail.height

                Text {
                    id: knobLabel

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: Theme.deckTextColor
                    font.pixelSize: 10
                    text: parent.modelData.label
                }
                Skin.ControlKnob {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: knobLabel.bottom
                    anchors.topMargin: 2
                    color: Theme.blue
                    group: root.groupResolved
                    height: width
                    key: "synth_step_" + (root.selectedStep + 1) + "_" + parent.modelData.key
                    width: Math.min(parent.width, detail.height * 0.7)
                }
            }
        }
        Skin.Button {
            anchors.verticalCenter: parent.verticalCenter
            height: detail.height * 0.8
            text: "-"
            width: detail.height * 0.8

            onClicked: selectedNote.value = Math.max(0, Math.round(selectedNote.value) - 1)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.blue
            font.pixelSize: 16
            text: root.noteName(selectedNote.value)
        }
        Skin.Button {
            anchors.verticalCenter: parent.verticalCenter
            height: detail.height * 0.8
            text: "+"
            width: detail.height * 0.8

            onClicked: selectedNote.value = Math.min(127, Math.round(selectedNote.value) + 1)
        }
    }

    // One cell of the synth lane. A tap toggles the step; a drag up or down
    // moves its note a semitone per semitonePx. The face is a Skin.Button for
    // the chrome, but the MouseArea above it owns the input so a drag is not
    // swallowed as a click.
    component SynthCell: Item {
        id: cell

        property bool dragged: false
        required property int index
        property real pressY: 0
        readonly property real semitonePx: Math.max(8, height * 0.25)
        property int startNote: 48

        Mixxx.ControlProxy {
            id: enabledControl

            group: root.groupResolved
            key: "synth_step_" + (cell.index + 1) + "_enabled"
        }
        Mixxx.ControlProxy {
            id: noteControl

            group: root.groupResolved
            key: "synth_step_" + (cell.index + 1) + "_note"
        }
        Skin.Button {
            activeColor: Theme.blue
            anchors.fill: parent
            fontPixelSize: 12
            highlight: enabledControl.value > 0
            // Beats read as groups of four.
            normalColor: Math.floor(cell.index / 4) % 2 ? Theme.darkGray4 : Theme.darkGray2
            text: root.noteName(noteControl.value)
        }
        Rectangle {
            anchors.fill: parent
            border.color: Theme.blue
            border.width: 2
            color: "transparent"
            radius: 3
            visible: root.selectedStep === cell.index
        }
        MouseArea {
            anchors.fill: parent

            onPositionChanged: mouse => {
                const delta = Math.round((cell.pressY - mouse.y) / cell.semitonePx);
                if (delta !== 0)
                    cell.dragged = true;
                if (cell.dragged)
                    noteControl.value = Math.max(0, Math.min(127, cell.startNote + delta));
            }
            onPressed: mouse => {
                cell.pressY = mouse.y;
                cell.startNote = Math.round(noteControl.value);
                cell.dragged = false;
                root.selectedStep = cell.index;
            }
            onReleased: {
                if (!cell.dragged)
                    enabledControl.value = enabledControl.value > 0 ? 0 : 1;
            }
        }
    }
}
