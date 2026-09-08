pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// A playable synthesizer: the [SynthN] engine channel's controls above a
// multi-touch keyboard. spec fields:
//   group     the synth group (default "[Synth1]")
//   octaves   keyboard span (default 2)
//   baseNote  MIDI note of the lowest key (default 48 = C3)
// Keys write note_on / note_off with the MIDI note number; the engine keeps
// the key state, so a MIDI keyboard mapped to the same group can play at the
// same time.
Item {
    id: root

    readonly property int baseNote: spec.baseNote ?? 48
    // C# D# _ F# G# A# _ : which white keys have a black key to their right
    readonly property var blackAfterWhite: [true, true, false, true, true, true, false]
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "[Synth1]") : (spec.group ?? "[Synth1]")
    // note -> true for every key currently down on THIS panel
    property var heldNotes: ({})
    readonly property int octaves: Math.max(1, spec.octaves ?? 2)
    // touch point id -> note, so each finger releases only its own key
    property var pointNotes: ({})
    required property var spec
    property var surface: null
    readonly property var waveNames: ["SINE", "TRI", "SAW", "SQR"]
    readonly property int whiteKeyCount: octaves * 7 + 1
    readonly property var whiteOffsets: [0, 2, 4, 5, 7, 9, 11]

    function hasBlackAfter(whiteIndex) {
        return whiteIndex < whiteKeyCount - 1 && blackAfterWhite[whiteIndex % 7];
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
    Row {
        id: controls

        // 17 controls, of which the two oscillator buttons are wider: their
        // label carries a waveform name ("1 SINE", "2 SQR"), which does not fit
        // in a square the size of a knob and was being clipped. Counting them
        // as 1.8 slots each keeps the row exactly as wide as it was.
        readonly property real knobSize: Math.min(height * 0.72, (root.width - spacing * 16) / 18.6)
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
                readonly property bool down: root.heldNotes[root.whiteNote(index)] === true
                required property int index
                readonly property bool isC: index % 7 === 0

                color: down ? Theme.blue : "#e8e8e8"
                height: keyboard.height
                radius: 3
                width: keyboard.whiteWidth - keyboard.gap
                x: index * keyboard.whiteWidth

                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: parent.down ? Theme.pureWhite : "#8a8a8a"
                    font.pixelSize: Math.max(9, keyboard.whiteWidth * 0.2)
                    text: "C" + (Math.floor(root.whiteNote(parent.index) / 12) - 1)
                    visible: parent.isC
                }
            }
        }
        Repeater {
            model: root.whiteKeyCount - 1

            Rectangle {
                readonly property bool down: root.heldNotes[root.whiteNote(index) + 1] === true
                required property int index

                color: down ? Theme.blue : "#141414"
                height: keyboard.blackHeight
                radius: 2
                visible: root.hasBlackAfter(index)
                width: keyboard.blackWidth
                x: (index + 1) * keyboard.whiteWidth - keyboard.blackWidth / 2

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: parent.down ? Theme.pureWhite : "#2a2a2a"
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
