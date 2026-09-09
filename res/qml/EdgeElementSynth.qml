pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// A playable synthesizer: the [SynthN] engine channel's controls above a
// multi-touch keyboard. spec fields:
//   group     the synth group (default "[Synth1]")
//   octaves   keyboard span (default 2)
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
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "[Synth1]") : (spec.group ?? "[Synth1]")
    // note -> true for every key currently down on THIS panel
    property var heldNotes: ({})
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
    required property var spec
    property var surface: null
    readonly property var waveNames: ["SINE", "TRI", "SAW", "SQR"]
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
    Row {
        id: controls

        // 21 controls of three widths. A knob is one slot; the two oscillator
        // buttons and the scale button are 1.8, because their label carries a
        // word ("1 SINE", "BLUES") that does not fit in a square; the key
        // button is 1.4, for two characters and a sharp. The octave buttons
        // are a single glyph and fit a square.
        readonly property real keyWidth: knobSize * 1.4
        readonly property real knobSize: Math.min(height * 0.72, (root.width - spacing * 20) / 23.8)
        readonly property real waveWidth: knobSize * 1.8

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.height * 0.32
        spacing: Math.max(4, root.width * 0.006)

        Skin.ControlButton {
            activeColor: Theme.green
            group: root.groupResolved
            height: controls.knobSize
            key: "main_mix"
            text: "ON"
            toggleable: true
            width: controls.knobSize
        }
        Skin.Button {
            activeColor: Theme.blue
            height: controls.knobSize
            highlight: true
            text: "−"
            width: controls.knobSize

            onClicked: root.shiftOctave(-1)
        }
        Skin.Button {
            activeColor: Theme.blue
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
            height: controls.knobSize
            highlight: true
            text: root.noteNames[Math.round(scaleRootControl.value) % 12]
            width: controls.keyWidth

            onClicked: root.cycleRoot()
        }
        Skin.Button {
            activeColor: Theme.blue
            height: controls.knobSize
            highlight: true
            text: root.scaleLabel()
            width: controls.waveWidth

            onClicked: root.cycleScale()
        }
        Skin.Button {
            activeColor: Theme.amber
            height: controls.knobSize
            highlight: true
            text: "1 " + root.waveNames[Math.max(0, Math.min(3, Math.round(osc1Wave.value)))]
            width: controls.waveWidth

            onClicked: osc1Wave.value = (Math.round(osc1Wave.value) + 1) % 4
        }
        Skin.Button {
            activeColor: Theme.amber
            height: controls.knobSize
            highlight: true
            text: "2 " + root.waveNames[Math.max(0, Math.min(3, Math.round(osc2Wave.value)))]
            width: controls.waveWidth

            onClicked: osc2Wave.value = (Math.round(osc2Wave.value) + 1) % 4
        }
        Repeater {
            model: [
                {
                    "key": "osc_mix",
                    "label": "MIX",
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

            Item {
                required property var modelData

                height: controls.height
                width: controls.knobSize

                Text {
                    id: knobLabel

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: Theme.deckTextColor
                    font.pixelSize: Math.max(9, controls.height * 0.14)
                    text: parent.modelData.label
                }
                Skin.ControlKnob {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: knobLabel.bottom
                    anchors.topMargin: 2
                    color: parent.modelData.color
                    group: root.groupResolved
                    height: width
                    key: parent.modelData.key
                    width: Math.min(parent.width, controls.height * 0.72)
                }
            }
        }
    }
    Item {
        id: keyboard

        readonly property real blackHeight: height * 0.6
        readonly property real blackWidth: whiteWidth * 0.58
        readonly property real gap: Math.max(1, whiteWidth * 0.03)
        readonly property real whiteWidth: width / root.whiteKeyCount

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: controls.bottom
        anchors.topMargin: Math.max(4, root.height * 0.02)

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
}
