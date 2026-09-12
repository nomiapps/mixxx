pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// A playable synthesizer: the [SynthN] engine channel's controls above a
// multi-touch keyboard. spec fields:
//   group     the synth group (default "[Synth1]")
//   octaves   keyboard span (default 2)
//   wavetable the wavetable band between the controls and the keyboard:
//             the table picker, its frame count and the stack of frames
//             with the one WT POS is playing drawn bright (default true;
//             hidden anyway where the band cannot fit)
//   compact   the main-window arrangement: a fixed-height control row,
//             then the wavetable band and the keyboard SIDE BY SIDE, the
//             keys no wider than they need to be. The Edge panel stacks
//             them instead and scales everything with its height
//             (default false)
// The space left beside the frame stack (Edge) or beside the keys (compact)
// holds the extras: the LFO with its shape display, and what later
// landings add there.
// Keys write note_on / note_off with the MIDI note number; the engine keeps
// the key state, so a MIDI keyboard mapped to the same group can play at the
// same time.
//
// Where the keyboard starts and what key it is in are NOT spec fields: they
// are the synth's own scale_mask / scale_root / base_note controls, shared
// with every other surface playing it. Moving the octave here moves a pad
// controller with it, and out-of-scale keys grey out on both.
Item {
    id: root

    readonly property int baseNote: Math.round(baseNoteControl.value)
    // C# D# _ F# G# A# _ : which white keys have a black key to their right
    readonly property var blackAfterWhite: [true, true, false, true, true, true, false]
    readonly property bool compact: spec.compact ?? false
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "[Synth1]") : (spec.group ?? "[Synth1]")
    // note -> true for every key currently down on THIS panel
    property var heldNotes: ({})
    // The beat divisions lfo_rate picks when synced, in the engine's order.
    readonly property var lfoDivisionNames: ["4 BAR", "2 BAR", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/32"]
    readonly property var lfoShapeNames: ["SINE", "TRI", "SAW", "SQR", "S&H"]
    readonly property var lfoTargetNames: ["OFF", "WT", "CUT", "PIT"]
    readonly property var noteNames: ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    readonly property int octaves: Math.max(1, spec.octaves ?? 2)
    // touch point id -> note, so each finger releases only its own key
    property var pointNotes: ({})
    // The scales this panel can select. The control holds a mask, not an index
    // into this table, so a controller offering a different set of scales
    // still lights up the right notes; only the NAME needs the table.
    readonly property var scaleTable: [
        {
            "name": "CHROM",
            "degrees": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
        },
        {
            "name": "MAJOR",
            "degrees": [0, 2, 4, 5, 7, 9, 11]
        },
        {
            "name": "MINOR",
            "degrees": [0, 2, 3, 5, 7, 8, 10]
        },
        {
            "name": "DORIAN",
            "degrees": [0, 2, 3, 5, 7, 9, 10]
        },
        {
            "name": "PENT",
            "degrees": [0, 3, 5, 7, 10]
        },
        {
            "name": "BLUES",
            "degrees": [0, 3, 5, 6, 7, 10]
        }
    ]
    readonly property bool showWavetable: (spec.wavetable ?? true) && root.height >= (compact ? 120 : 200)
    required property var spec
    property var surface: null
    readonly property var waveNames: ["SINE", "TRI", "SAW", "SQR", "WT"]
    // Bumped whenever the synth reports a table change, so the name below
    // re-reads; the singleton has no per-group property to bind to.
    property int wavetableEpoch: 0
    readonly property string wavetableName: root.wavetableEpoch >= 0 ? Mixxx.Synth.wavetableName(root.groupResolved) : ""
    readonly property int whiteKeyCount: octaves * 7 + 1
    readonly property var whiteOffsets: [0, 2, 4, 5, 7, 9, 11]

    // Step the root up a semitone, wrapping at the octave.
    function cycleRoot() {
        releaseAll();
        scaleRootControl.value = (Math.round(scaleRootControl.value) + 1) % 12;
    }
    // Step to the next scale in the table. A mask that is not in the table --
    // set by a controller with scales of its own -- restarts from the top.
    function cycleScale() {
        releaseAll();
        const current = scaleIndex();
        scaleMaskControl.value = maskFor(scaleTable[(current + 1) % scaleTable.length].degrees);
    }
    function hasBlackAfter(whiteIndex) {
        return whiteIndex < whiteKeyCount - 1 && blackAfterWhite[whiteIndex % 7];
    }
    // Semitone offsets to the 12-bit mask the control carries.
    function maskFor(degrees) {
        let mask = 0;
        for (let i = 0; i < degrees.length; ++i)
            mask |= 1 << degrees[i];
        return mask;
    }
    function noteAt(x, y) {
        if (x < 0 || x >= keyboard.width || y < 0 || y >= keyboard.height)
            return -1;

        const whiteIndex = Math.floor(x / keyboard.whiteWidth);
        if (y < keyboard.blackHeight) {
            const inKey = x - whiteIndex * keyboard.whiteWidth;
            if (inKey > keyboard.whiteWidth - keyboard.blackWidth / 2 && hasBlackAfter(whiteIndex))
                return whiteNote(whiteIndex) + 1;
            if (inKey < keyboard.blackWidth / 2 && whiteIndex > 0 && hasBlackAfter(whiteIndex - 1))
                return whiteNote(whiteIndex - 1) + 1;
        }
        return whiteNote(whiteIndex);
    }
    // Whether a note is in the selected scale. A chromatic mask puts every
    // note in it, so the default state greys nothing out.
    function noteInScale(note) {
        const pitchClass = ((note - Math.round(scaleRootControl.value)) % 12 + 12) % 12;
        return (scaleMask() & (1 << pitchClass)) !== 0;
    }
    function noteIsRoot(note) {
        return ((note - Math.round(scaleRootControl.value)) % 12 + 12) % 12 === 0;
    }
    // Drop every key this panel is holding. Anything that moves the keys out
    // from under the fingers has to, or the notes are stranded on.
    function releaseAll() {
        pointNotes = ({});
        setHeld({});
    }
    // Which table entry the mask matches, or -1 for a mask set elsewhere.
    function scaleIndex() {
        const mask = scaleMask();
        for (let i = 0; i < scaleTable.length; ++i) {
            if (maskFor(scaleTable[i].degrees) === mask)
                return i;
        }
        return -1;
    }
    function scaleLabel() {
        const index = scaleIndex();
        return index < 0 ? "CUSTOM" : scaleTable[index].name;
    }
    // Guarded, so a control left at zero cannot grey out the whole keyboard:
    // no scale set means every note is in it.
    function scaleMask() {
        const mask = Math.round(scaleMaskControl.value) & 0xFFF;
        return mask > 0 ? mask : 4095;
    }
    function setHeld(next) {
        for (const note in next) {
            if (!heldNotes[note])
                noteOnControl.value = Number(note);
        }
        for (const note in heldNotes) {
            if (!next[note])
                noteOffControl.value = Number(note);
        }
        heldNotes = next;
    }
    // Move the whole keyboard by an octave, within MIDI range.
    function shiftOctave(direction) {
        const next = baseNote + direction * 12;
        if (next < 0 || next + octaves * 12 > 127)
            return;

        releaseAll();
        baseNoteControl.value = next;
    }
    // points: the touch points that changed; gone: they were lifted
    function track(points, gone) {
        const next = Object.assign({}, pointNotes);
        for (let i = 0; i < points.length; ++i) {
            const p = points[i];
            const note = gone ? -1 : noteAt(p.x, p.y);
            if (note >= 0)
                next[p.pointId] = note;
            else
                delete next[p.pointId];
        }
        pointNotes = next;
        const held = {};
        for (const id in next)
            held[next[id]] = true;
        setHeld(held);
    }
    function whiteNote(whiteIndex) {
        return baseNote + Math.floor(whiteIndex / 7) * 12 + whiteOffsets[whiteIndex % 7];
    }

    Mixxx.ControlProxy {
        id: noteOnControl

        group: root.groupResolved
        key: "note_on"
    }
    Mixxx.ControlProxy {
        id: noteOffControl

        group: root.groupResolved
        key: "note_off"
    }
    Mixxx.ControlProxy {
        id: osc1Wave

        group: root.groupResolved
        key: "osc1_wave"
    }
    Mixxx.ControlProxy {
        id: osc2Wave

        group: root.groupResolved
        key: "osc2_wave"
    }
    Mixxx.ControlProxy {
        id: baseNoteControl

        group: root.groupResolved
        key: "base_note"
    }
    Mixxx.ControlProxy {
        id: scaleMaskControl

        group: root.groupResolved
        key: "scale_mask"
    }
    Mixxx.ControlProxy {
        id: scaleRootControl

        group: root.groupResolved
        key: "scale_root"
    }
    Mixxx.ControlProxy {
        id: wtPositionControl

        group: root.groupResolved
        key: "wt_position"
    }
    Mixxx.ControlProxy {
        id: wtFramesControl

        group: root.groupResolved
        key: "wt_frames"
    }
    Mixxx.ControlProxy {
        id: lfoShapeControl

        group: root.groupResolved
        key: "lfo_shape"
    }
    Mixxx.ControlProxy {
        id: lfoTargetControl

        group: root.groupResolved
        key: "lfo_target"
    }
    Mixxx.ControlProxy {
        id: lfoRateControl

        group: root.groupResolved
        key: "lfo_rate"
    }
    Mixxx.ControlProxy {
        id: lfoSyncControl

        group: root.groupResolved
        key: "lfo_sync"
    }
    Mixxx.ControlProxy {
        id: unisonVoicesControl

        group: root.groupResolved
        key: "unison_voices"
    }
    Mixxx.ControlProxy {
        id: patchControl

        group: root.groupResolved
        key: "patch"
    }
    Connections {
        function onWavetableChanged(group) {
            if (group === root.groupResolved)
                root.wavetableEpoch += 1;
        }

        target: Mixxx.Synth
    }
    Row {
        id: controls

        // 23 controls of three widths. A knob is one slot; the two oscillator
        // buttons and the scale button are 1.8, because their label carries a
        // word ("1 SINE", "BLUES") that does not fit in a square; the key
        // button is 1.4, for two characters and a sharp. The octave buttons
        // are a single glyph and fit a square.
        // Button text grows with the buttons: 16 px on the Edge, where they
        // are the size of a fingertip, down to 10 px in the compact row.
        readonly property int buttonFont: knobSize >= 72 ? 16 : (knobSize >= 52 ? 12 : 10)
        readonly property real keyWidth: knobSize * 1.4
        readonly property real knobSize: Math.min(height * 0.72, (root.width - spacing * 22) / 25.8)
        readonly property real waveWidth: knobSize * 1.8

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.compact ? 56 : root.height * 0.32
        spacing: Math.max(4, root.width * 0.006)

        Skin.ControlButton {
            activeColor: Theme.green
            fontPixelSize: controls.buttonFont
            group: root.groupResolved
            height: controls.knobSize
            key: "main_mix"
            text: "ON"
            toggleable: true
            width: controls.knobSize
        }
        Skin.Button {
            activeColor: Theme.blue
            fontPixelSize: controls.buttonFont
            height: controls.knobSize
            highlight: true
            text: "−"
            width: controls.knobSize

            onClicked: root.shiftOctave(-1)
        }
        Skin.Button {
            activeColor: Theme.blue
            fontPixelSize: controls.buttonFont
            height: controls.knobSize
            highlight: true
            text: "+"
            width: controls.knobSize

            onClicked: root.shiftOctave(1)
        }
        // The key and the scale are the synth's, not this panel's: a pad
        // controller playing the same synth relights itself from them.
        Skin.Button {
            activeColor: Theme.blue
            fontPixelSize: controls.buttonFont
            height: controls.knobSize
            highlight: true
            text: root.noteNames[Math.round(scaleRootControl.value) % 12]
            width: controls.keyWidth

            onClicked: root.cycleRoot()
        }
        Skin.Button {
            activeColor: Theme.blue
            fontPixelSize: controls.buttonFont
            height: controls.knobSize
            highlight: true
            text: root.scaleLabel()
            width: controls.waveWidth

            onClicked: root.cycleScale()
        }
        Skin.Button {
            activeColor: Theme.amber
            fontPixelSize: controls.buttonFont
            height: controls.knobSize
            highlight: true
            text: "1 " + root.waveNames[Math.max(0, Math.min(4, Math.round(osc1Wave.value)))]
            width: controls.waveWidth

            onClicked: osc1Wave.value = (Math.round(osc1Wave.value) + 1) % 5
        }
        Skin.Button {
            activeColor: Theme.amber
            fontPixelSize: controls.buttonFont
            height: controls.knobSize
            highlight: true
            text: "2 " + root.waveNames[Math.max(0, Math.min(4, Math.round(osc2Wave.value)))]
            width: controls.waveWidth

            onClicked: osc2Wave.value = (Math.round(osc2Wave.value) + 1) % 5
        }
        Repeater {
            model: [
                {
                    "key": "osc_mix",
                    "label": "MIX",
                    "color": Theme.amber
                },
                {
                    "key": "wt_position",
                    "label": "WT POS",
                    "color": Theme.amber
                },
                {
                    "key": "wt_env_amount",
                    "label": "WT ENV",
                    "color": Theme.amber
                },
                {
                    "key": "osc2_semitones",
                    "label": "SEMI",
                    "color": Theme.amber
                },
                {
                    "key": "osc2_detune",
                    "label": "DETUNE",
                    "color": Theme.amber
                },
                {
                    "key": "cutoff",
                    "label": "CUTOFF",
                    "color": Theme.blue
                },
                {
                    "key": "resonance",
                    "label": "RES",
                    "color": Theme.blue
                },
                {
                    "key": "env_amount",
                    "label": "ENV",
                    "color": Theme.blue
                },
                {
                    "key": "attack",
                    "label": "A",
                    "color": Theme.green
                },
                {
                    "key": "decay",
                    "label": "D",
                    "color": Theme.green
                },
                {
                    "key": "sustain",
                    "label": "S",
                    "color": Theme.green
                },
                {
                    "key": "release",
                    "label": "R",
                    "color": Theme.green
                },
                {
                    "key": "pregain",
                    "label": "GAIN",
                    "color": Theme.deckActiveColor
                },
                {
                    "key": "volume",
                    "label": "VOL",
                    "color": Theme.deckActiveColor
                }
            ]

            KnobCell {
                required property var modelData

                cell: controls.height
                knobColor: modelData.color
                knobKey: modelData.key
                label: modelData.label
            }
        }
    }
    // The wavetable band: picker on the left, the stack of frames beside
    // it. Only when there is room. Stacked above the keyboard on the Edge;
    // in the compact row it takes the left of the space under the controls
    // and the keyboard sits beside it.
    Item {
        id: wavetableBand

        // The stack reads well at about 2.5:1; in the compact row it is
        // given that width and the keys get the rest.
        readonly property real stackWidth: root.compact ? 240 : width * 0.3

        anchors.left: parent.left
        anchors.top: controls.bottom
        anchors.topMargin: root.showWavetable ? Math.max(4, root.height * 0.02) : 0
        height: !root.showWavetable ? 0 : (root.compact ? root.height - controls.height - anchors.topMargin : root.height * 0.26)
        visible: root.showWavetable
        width: root.compact ? picker.width + controls.spacing + stackWidth : root.width

        Row {
            anchors.fill: parent
            spacing: controls.spacing

            Column {
                id: picker

                // 44 px for touch where the band is tall; in the main-window row
                // the band is a third of that and the buttons shrink to fit it.
                readonly property real buttonHeight: Math.max(22, Math.min(44, (wavetableBand.height - spacing * 2 - framesLabel.height) / 2))

                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                width: controls.waveWidth * 1.6

                Skin.Button {
                    activeColor: Theme.amber
                    fontPixelSize: 14
                    height: picker.buttonHeight
                    highlight: true
                    text: root.wavetableName
                    width: parent.width

                    onClicked: Mixxx.Synth.stepWavetable(root.groupResolved, 1)
                }
                Row {
                    spacing: 4

                    Skin.Button {
                        fontPixelSize: 16
                        height: picker.buttonHeight
                        text: "\u2039"
                        width: (picker.width - parent.spacing) / 2

                        onClicked: Mixxx.Synth.stepWavetable(root.groupResolved, -1)
                    }
                    Skin.Button {
                        fontPixelSize: 16
                        height: picker.buttonHeight
                        text: "\u203a"
                        width: (picker.width - parent.spacing) / 2

                        onClicked: Mixxx.Synth.stepWavetable(root.groupResolved, 1)
                    }
                }
                Text {
                    id: framesLabel

                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.deckTextColor
                    font.pixelSize: 12
                    text: Math.round(wtFramesControl.value) + " FRAMES"
                }
            }
            Rectangle {
                border.color: Theme.panelBorderColor
                color: Theme.sunkenBackgroundColor
                height: wavetableBand.height
                radius: 3
                width: wavetableBand.stackWidth

                Mixxx.WavetableView {
                    anchors.fill: parent
                    anchors.margins: 6
                    currentColor: Theme.wavetableCurrentColor
                    frameColor: Theme.wavetableFrameColor
                    group: root.groupResolved
                    position: wtPositionControl.value
                }
            }
        }
    }
    // The extras: the LFO now; unison, patches and the curves follow. On
    // the Edge this is the part of the band to the right of the frame
    // stack; in the compact row it is the space to the right of the keys.
    Item {
        id: extras

        readonly property real buttonWidth: line * 0.72 * 1.8
        readonly property int font: knob >= 72 ? 16 : (knob >= 52 ? 12 : 10)
        readonly property real knob: line * 0.72
        readonly property real line: (height - controls.spacing) / 2

        anchors.bottom: root.compact ? parent.bottom : wavetableBand.bottom
        anchors.left: root.compact ? keyboard.right : parent.left
        anchors.leftMargin: root.compact ? controls.spacing * 2 : picker.width + wavetableBand.stackWidth + controls.spacing * 3
        anchors.right: parent.right
        anchors.top: root.compact ? controls.bottom : wavetableBand.top
        anchors.topMargin: root.compact ? wavetableBand.anchors.topMargin : 0
        clip: true
        visible: root.showWavetable

        Column {
            anchors.fill: parent
            spacing: controls.spacing

            Row {
                height: extras.line
                spacing: controls.spacing

                Skin.Button {
                    activeColor: Theme.purple
                    anchors.bottom: parent.bottom
                    fontPixelSize: extras.font
                    height: extras.knob
                    highlight: true
                    text: "LFO " + root.lfoShapeNames[Math.max(0, Math.min(4, Math.round(lfoShapeControl.value)))]
                    width: extras.buttonWidth

                    onClicked: lfoShapeControl.value = (Math.round(lfoShapeControl.value) + 1) % 5
                }
                KnobCell {
                    cell: extras.line
                    knobColor: Theme.purple
                    knobKey: "lfo_rate"
                    label: "RATE"
                }
                KnobCell {
                    cell: extras.line
                    knobColor: Theme.purple
                    knobKey: "lfo_depth"
                    label: "DEPTH"
                }
                Skin.Button {
                    activeColor: Theme.purple
                    anchors.bottom: parent.bottom
                    fontPixelSize: extras.font
                    height: extras.knob
                    highlight: true
                    text: "TO " + root.lfoTargetNames[Math.max(0, Math.min(3, Math.round(lfoTargetControl.value)))]
                    width: extras.buttonWidth

                    onClicked: lfoTargetControl.value = (Math.round(lfoTargetControl.value) + 1) % 4
                }
                // Lit when synced, reading the division lfo_rate picks; unlit
                // it runs free and the knob is a frequency.
                Skin.ControlButton {
                    activeColor: Theme.purple
                    anchors.bottom: parent.bottom
                    fontPixelSize: extras.font
                    group: root.groupResolved
                    height: extras.knob
                    key: "lfo_sync"
                    text: lfoSyncControl.value > 0 ? root.lfoDivisionNames[Math.max(0, Math.min(7, Math.round(lfoRateControl.value * 7)))] : "FREE"
                    toggleable: true
                    width: extras.buttonWidth
                }
                Skin.SynthLfoShape {
                    anchors.bottom: parent.bottom
                    group: root.groupResolved
                    height: extras.knob
                    lineColor: Theme.purple
                    shape: Math.max(0, Math.min(4, Math.round(lfoShapeControl.value)))
                    width: extras.knob * 2.2
                }
                // Unison: how many voices a note gets, spread by DETUNE and
                // panned apart by WIDTH.
                Skin.Button {
                    activeColor: Theme.purple
                    anchors.bottom: parent.bottom
                    fontPixelSize: extras.font
                    height: extras.knob
                    highlight: true
                    text: Math.max(1, Math.min(4, Math.round(unisonVoicesControl.value))) + " VOICE"
                    width: extras.buttonWidth

                    onClicked: unisonVoicesControl.value = (Math.round(unisonVoicesControl.value) % 4) + 1
                }
                KnobCell {
                    cell: extras.line
                    knobColor: Theme.purple
                    knobKey: "unison_detune"
                    label: "DETUNE"
                }
                KnobCell {
                    cell: extras.line
                    knobColor: Theme.purple
                    knobKey: "unison_spread"
                    label: "WIDTH"
                }
            }
            Row {
                height: extras.line
                spacing: controls.spacing

                // The envelope and the filter as they will sound, in their
                // knobs' colours.
                Skin.SynthEnvelopeCurve {
                    anchors.bottom: parent.bottom
                    group: root.groupResolved
                    height: extras.knob
                    lineColor: Theme.green
                    width: extras.knob * 2.2
                }
                Skin.SynthFilterCurve {
                    anchors.bottom: parent.bottom
                    group: root.groupResolved
                    height: extras.knob
                    lineColor: Theme.blue
                    width: extras.knob * 2.2
                }
                // Patch slots: the current one lit, filled ones on the lighter
                // face, empty ones on the darker. Selecting a filled slot loads
                // it; SAVE snapshots the live sound into the current slot.
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: (extras.knob - height) / 2
                    color: Theme.deckTextColor
                    font.pixelSize: extras.font
                    text: "PATCH"
                }
                Repeater {
                    model: 8

                    Skin.Button {
                        id: patchSlot

                        required property int index

                        activeColor: Theme.amber
                        anchors.bottom: parent.bottom
                        fontPixelSize: extras.font
                        height: extras.knob
                        highlight: Math.round(patchControl.value) === index + 1
                        normalColor: patchFilled.value > 0 ? Theme.darkGray2 : Theme.darkGray4
                        text: index + 1
                        width: extras.knob

                        onClicked: patchControl.value = index + 1

                        Mixxx.ControlProxy {
                            id: patchFilled

                            group: root.groupResolved
                            key: "patch_" + (patchSlot.index + 1) + "_filled"
                        }
                    }
                }
                Skin.EdgePadButton {
                    activeColor: Theme.amber
                    anchors.bottom: parent.bottom
                    fontPixelSize: extras.font
                    height: extras.knob
                    padGroup: root.groupResolved
                    padKey: "patch_save"
                    text: "SAVE"
                    width: extras.buttonWidth
                }
            }
        }
    }
    Item {
        id: keyboard

        readonly property real blackHeight: height * 0.6
        readonly property real blackWidth: whiteWidth * 0.58
        readonly property real gap: Math.max(1, whiteWidth * 0.03)
        // On a mouse a white key needs no more than this; the Edge, sized
        // for fingers, spreads its keys over the full width instead.
        readonly property real maxWhiteWidth: 26
        readonly property real whiteWidth: width / root.whiteKeyCount

        anchors.bottom: parent.bottom
        anchors.left: root.compact && root.showWavetable ? wavetableBand.right : parent.left
        anchors.leftMargin: root.compact && root.showWavetable ? controls.spacing * 2 : 0
        anchors.top: root.compact ? controls.bottom : wavetableBand.bottom
        anchors.topMargin: Math.max(4, root.height * 0.02)
        width: root.compact ? Math.min(root.width - anchors.leftMargin - (root.showWavetable ? wavetableBand.width : 0), root.whiteKeyCount * maxWhiteWidth) : root.width

        Repeater {
            model: root.whiteKeyCount

            Rectangle {
                readonly property bool down: root.heldNotes[note] === true
                required property int index
                readonly property bool isC: index % 7 === 0
                readonly property int note: root.whiteNote(index)

                // A chromatic mask puts every note in the scale, so by default
                // nothing is greyed and this looks like a plain keyboard.
                color: down ? Theme.blue : (root.noteInScale(note) ? Theme.offWhite : "#9d9d9d")
                height: keyboard.height
                radius: 3
                width: keyboard.whiteWidth - keyboard.gap
                x: index * keyboard.whiteWidth

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    color: Theme.amber
                    height: 4
                    radius: 2
                    visible: root.noteIsRoot(parent.note)
                    width: parent.width - 8
                }
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: parent.down ? Theme.pureWhite : "#8a8a8a"
                    font.pixelSize: Math.max(9, keyboard.whiteWidth * 0.2)
                    text: "C" + (Math.floor(parent.note / 12) - 1)
                    visible: parent.isC
                }
            }
        }
        Repeater {
            model: root.whiteKeyCount - 1

            Rectangle {
                readonly property bool down: root.heldNotes[note] === true
                required property int index
                readonly property int note: root.whiteNote(index) + 1

                color: down ? Theme.blue : "#141414"
                height: keyboard.blackHeight
                radius: 2
                visible: root.hasBlackAfter(index)
                width: keyboard.blackWidth
                x: (index + 1) * keyboard.whiteWidth - keyboard.blackWidth / 2

                // Dimming a black key does not read, so the strip along its
                // foot carries the scale instead: lit when the note is in it,
                // amber on the root, all but invisible when it is out.
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: parent.down ? Theme.pureWhite : (root.noteIsRoot(parent.note) ? Theme.amber : (root.noteInScale(parent.note) ? "#5a5a5a" : "#1a1a1a"))
                    height: 3
                    radius: 1
                    width: parent.width - 8
                }
            }
        }
        MultiPointTouchArea {
            anchors.fill: parent
            mouseEnabled: true

            onCanceled: touchPoints => root.track(touchPoints, true)
            onPressed: touchPoints => root.track(touchPoints, false)
            onReleased: touchPoints => root.track(touchPoints, true)
            onUpdated: touchPoints => root.track(touchPoints, false)
        }
    }

    // A labelled knob: the label above, the knob below, the slot as wide
    // as the wider of the two. Used by the control row and the extras.
    component KnobCell: Item {
        id: cellRoot

        required property real cell
        required property color knobColor
        required property string knobKey
        required property string label

        height: cell
        width: Math.max(cell * 0.72, cellLabel.implicitWidth + 4)

        Text {
            id: cellLabel

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            color: Theme.deckTextColor
            font.pixelSize: Math.max(10, cellRoot.cell * 0.14)
            text: cellRoot.label
        }
        Skin.ControlKnob {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: cellLabel.bottom
            anchors.topMargin: 2
            color: cellRoot.knobColor
            group: root.groupResolved
            height: width
            key: cellRoot.knobKey
            width: Math.min(parent.width, cellRoot.cell * 0.72)
        }
    }
}
