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

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")

    readonly property int rows: spec.rows ?? 2
    readonly property int columns: spec.columns ?? 4
    readonly property int padCount: rows * columns
    readonly property var modes: spec.modes ?? ["hotcue"]
    readonly property real gap: Math.max(3, height * 0.04)
    property int modeIndex: 0
    readonly property string mode: modes[modeIndex]

    readonly property var loopSizes: ["0.5", "1", "2", "4", "8", "16", "32", "64"]
    readonly property var stemNames: ["DRUMS", "BASS", "OTHER", "VOCALS"]
    readonly property var stemColors: ["#35b592", "#d78f2c", "#d075b7", "#46a3d9"]

    function stemGroup(index) {
        const g = root.groupResolved;
        return g && g.length > 2 ? g.slice(0, -1) + "_Stem" + (index + 1) + "]" : "";
    }
    readonly property var jumpSizes: ["1", "2", "4", "8"]
    // FLX4-style split: deck 2 declares samplerOffset 4 to get samplers 5..8
    readonly property int samplerOffset: spec.samplerOffset ?? 0

    function modeColor(name) {
        switch (name) {
        case "beatloop":
            return Theme.bpmSliderBarColor;
        case "beatjump":
            return "#e0a040";
        case "sampler":
            return "#b060e0";
        case "stems":
            return "#46a3d9";
        default:
            return Theme.deckActiveColor;
        }
    }

    Row {
        id: modeRow

        visible: root.modes.length > 1
        height: visible ? root.height * 0.2 : 0
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: root.gap

        Repeater {
            model: root.modes

            Skin.Button {
                required property int index
                required property string modelData

                width: (root.width - root.gap * (root.modes.length - 1)) / root.modes.length
                height: modeRow.height
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
                activeColor: root.modeColor(modelData)
                highlight: root.modeIndex === index
                onClicked: root.modeIndex = index
            }
        }
    }

    Item {
        id: padArea

        anchors.top: modeRow.bottom
        anchors.topMargin: root.modes.length > 1 ? root.gap : 0
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        readonly property real padWidth: (width - root.gap * (root.columns - 1)) / root.columns
        readonly property real padHeight: (height - root.gap * (root.rows - 1)) / root.rows

        Grid {
            visible: root.mode === "hotcue"
            columns: root.columns
            spacing: root.gap

            Repeater {
                model: root.padCount

                Skin.HotcueButton {
                    required property int index

                    width: padArea.padWidth
                    height: padArea.padHeight
                    hotcueNumber: index + 1
                    group: root.groupResolved
                }
            }
        }

        Grid {
            visible: root.mode === "beatloop"
            columns: root.columns
            spacing: root.gap

            Repeater {
                model: Math.min(root.padCount, root.loopSizes.length)

                Skin.EdgePadButton {
                    required property int index

                    width: padArea.padWidth
                    height: padArea.padHeight
                    text: index === 0 ? "1/2" : root.loopSizes[index]
                    activeColor: root.modeColor("beatloop")
                    padGroup: root.groupResolved
                    padKey: "beatloop_" + root.loopSizes[index] + "_toggle"
                    activeKey: "beatloop_" + root.loopSizes[index] + "_enabled"
                }
            }
        }

        Grid {
            visible: root.mode === "beatjump"
            columns: root.columns
            spacing: root.gap

            Repeater {
                model: Math.min(root.padCount, root.jumpSizes.length * 2)

                Skin.EdgePadButton {
                    required property int index

                    readonly property bool backward: index < root.jumpSizes.length
                    readonly property string jumpSize: root.jumpSizes[backward ? index : index - root.jumpSizes.length]

                    width: padArea.padWidth
                    height: padArea.padHeight
                    text: backward ? ("< " + jumpSize) : (jumpSize + " >")
                    activeColor: root.modeColor("beatjump")
                    padGroup: root.groupResolved
                    padKey: "beatjump_" + jumpSize + (backward ? "_backward" : "_forward")
                }
            }
        }

        Grid {
            visible: root.mode === "stems"
            columns: root.columns
            spacing: root.gap

            Repeater {
                model: 4

                Skin.ControlButton {
                    required property int index

                    width: padArea.padWidth
                    height: padArea.padHeight
                    group: root.stemGroup(index)
                    key: "mute"
                    text: root.stemNames[index]
                    toggleable: true
                    activeColor: root.stemColors[index]
                }
            }

            Repeater {
                model: 4

                Skin.ControlButton {
                    required property int index

                    width: padArea.padWidth
                    height: padArea.padHeight
                    group: "[QuickEffectRack1_" + root.stemGroup(index) + "]"
                    key: "enabled"
                    text: "FX " + root.stemNames[index].charAt(0)
                    toggleable: true
                    activeColor: root.stemColors[index]
                }
            }
        }

        Grid {
            visible: root.mode === "sampler"
            columns: root.columns
            spacing: root.gap

            Repeater {
                model: root.padCount

                Skin.EdgePadButton {
                    required property int index

                    width: padArea.padWidth
                    height: padArea.padHeight
                    text: "S" + (index + 1 + root.samplerOffset)
                    activeColor: root.modeColor("sampler")
                    padGroup: "[Sampler" + (index + 1 + root.samplerOffset) + "]"
                    padKey: "cue_gotoandplay"
                    activeKey: "play"
                }
            }
        }
    }
}
