pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.12
import "Theme"

// Performance pads with switchable banks, like hardware pad modes.
// spec.modes picks which banks exist (default hotcue only):
//   "hotcue"   - hotcues 1..N (set/trigger, lit with cue color)
//   "beatloop" - beatloop toggles 1/2..64 beats (lit while looping)
//   "beatjump" - jump back/forward 1/2/4/8 beats
//   "sampler"  - trigger samplers 1..N (lit while playing)
// A mode strip renders above the grid when more than one bank is declared.
Item {
    id: root

    readonly property int columns: spec.columns ?? 4
    readonly property real gap: Math.max(3, height * 0.04)
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    readonly property var jumpSizes: ["1", "2", "4", "8"]
    readonly property var loopSizes: ["0.5", "1", "2", "4", "8", "16", "32", "64"]
    readonly property string mode: modes[modeIndex]
    property int modeIndex: 0
    readonly property var modes: spec.modes ?? ["hotcue"]
    readonly property int padCount: rows * columns
    readonly property int rows: spec.rows ?? 2
    // FLX4-style split: deck 2 declares samplerOffset 4 to get samplers 5..8
    readonly property int samplerOffset: spec.samplerOffset ?? 0
    required property var spec
    readonly property var stemColors: ["#35b592", "#d78f2c", "#d075b7", "#46a3d9"]
    readonly property var stemNames: ["DRUMS", "BASS", "OTHER", "VOCALS"]
    property var surface: null

    function modeColor(name) {
        switch (name) {
        case "beatloop":
            return Theme.bpmSliderBarColor;
        case "beatjump":
            return Theme.amber;
        case "sampler":
            return Theme.purple;
        case "stems":
            return "#46a3d9";
        default:
            return Theme.deckActiveColor;
        }
    }
    function stemGroup(index) {
        const g = root.groupResolved;
        return g && g.length > 2 ? g.slice(0, -1) + "_Stem" + (index + 1) + "]" : "";
    }

    Row {
        id: modeRow

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        height: visible ? root.height * 0.2 : 0
        spacing: root.gap
        visible: root.modes.length > 1

        Repeater {
            model: root.modes

            Skin.Button {
                required property int index
                required property string modelData

                activeColor: root.modeColor(modelData)
                height: modeRow.height
                highlight: root.modeIndex === index
                text: {
                    switch (modelData) {
                    case "hotcue":
                        return "HOT CUE";
                    case "beatloop":
                        return "LOOP";
                    case "beatjump":
                        return "JUMP";
                    case "sampler":
                        return "SAMPLER";
                    default:
                        return modelData.toUpperCase();
                    }
                }
                width: (root.width - root.gap * (root.modes.length - 1)) / root.modes.length

                onClicked: root.modeIndex = index
            }
        }
    }
    Item {
        id: padArea

        readonly property real padHeight: (height - root.gap * (root.rows - 1)) / root.rows
        readonly property real padWidth: (width - root.gap * (root.columns - 1)) / root.columns

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: modeRow.bottom
        anchors.topMargin: root.modes.length > 1 ? root.gap : 0

        Grid {
            columns: root.columns
            spacing: root.gap
            visible: root.mode === "hotcue"

            Repeater {
                model: root.padCount

                Skin.HotcueButton {
                    required property int index

                    group: root.groupResolved
                    height: padArea.padHeight
                    hotcueNumber: index + 1
                    width: padArea.padWidth
                }
            }
        }
        Grid {
            columns: root.columns
            spacing: root.gap
            visible: root.mode === "beatloop"

            Repeater {
                model: Math.min(root.padCount, root.loopSizes.length)

                Skin.EdgePadButton {
                    required property int index

                    activeColor: root.modeColor("beatloop")
                    activeKey: "beatloop_" + root.loopSizes[index] + "_enabled"
                    height: padArea.padHeight
                    padGroup: root.groupResolved
                    padKey: "beatloop_" + root.loopSizes[index] + "_toggle"
                    text: index === 0 ? "1/2" : root.loopSizes[index]
                    width: padArea.padWidth
                }
            }
        }
        Grid {
            columns: root.columns
            spacing: root.gap
            visible: root.mode === "beatjump"

            Repeater {
                model: Math.min(root.padCount, root.jumpSizes.length * 2)

                Skin.EdgePadButton {
                    readonly property bool backward: index < root.jumpSizes.length
                    required property int index
                    readonly property string jumpSize: root.jumpSizes[backward ? index : index - root.jumpSizes.length]

                    activeColor: root.modeColor("beatjump")
                    height: padArea.padHeight
                    padGroup: root.groupResolved
                    padKey: "beatjump_" + jumpSize + (backward ? "_backward" : "_forward")
                    text: backward ? ("< " + jumpSize) : (jumpSize + " >")
                    width: padArea.padWidth
                }
            }
        }
        Grid {
            columns: root.columns
            spacing: root.gap
            visible: root.mode === "stems"

            Repeater {
                model: 4

                Skin.ControlButton {
                    required property int index

                    activeColor: root.stemColors[index]
                    group: root.stemGroup(index)
                    height: padArea.padHeight
                    key: "mute"
                    text: root.stemNames[index]
                    toggleable: true
                    width: padArea.padWidth
                }
            }
            Repeater {
                model: 4

                Skin.ControlButton {
                    required property int index

                    activeColor: root.stemColors[index]
                    group: "[QuickEffectRack1_" + root.stemGroup(index) + "]"
                    height: padArea.padHeight
                    key: "enabled"
                    text: "FX " + root.stemNames[index].charAt(0)
                    toggleable: true
                    width: padArea.padWidth
                }
            }
        }
        Grid {
            columns: root.columns
            spacing: root.gap
            visible: root.mode === "sampler"

            Repeater {
                model: root.padCount

                Skin.EdgePadButton {
                    required property int index

                    activeColor: root.modeColor("sampler")
                    activeKey: "play"
                    height: padArea.padHeight
                    padGroup: "[Sampler" + (index + 1 + root.samplerOffset) + "]"
                    padKey: "cue_gotoandplay"
                    text: "S" + (index + 1 + root.samplerOffset)
                    width: padArea.padWidth
                }
            }
        }
    }
}
