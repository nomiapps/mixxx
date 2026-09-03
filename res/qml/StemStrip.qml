import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

// A dedicated, full-width strip of per-stem controls (Volume, Mute, Solo and
// the quick-effect) that appears under a deck's waveform when a stem track is
// loaded. Separate from — and in addition to — the standard channel EQ.
Item {
    id: root

    required property string group
    property var player: Mixxx.PlayerManager.getPlayer(root.group)
    // Whether the host wants the strip shown at all (e.g. hidden when the
    // library is maximized). Combined with the actual stem count.
    property bool active: true
    // Index of the currently soloed stem, or -1 for none.
    property int soloedStem: -1

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }

    readonly property bool hasStems: stemCountControl.value > 0

    visible: root.active && root.hasStems
    implicitHeight: visible ? 52 : 0

    function stemGroup(index) {
        return `${root.group.substr(0, root.group.length - 1)}_Stem${index + 1}]`;
    }

    // Solo = exclusive mute: soloing a stem mutes the other three; clicking the
    // lit solo again clears it and unmutes all. Each cell applies this to its
    // own mute control when soloedStem changes.
    function toggleSolo(index) {
        root.soloedStem = (root.soloedStem === index) ? -1 : index;
    }

    // Small labelled toggle button used for Mute / Solo / FX-enable.
    component StemToggle: Rectangle {
        id: tgl

        property bool on: false
        property color accent: Theme.red
        property string glyph: ""
        signal toggled()

        width: 22
        height: 20
        radius: 3
        border.width: 1
        border.color: tgl.on
                ? tgl.accent
                : (tglMouse.containsMouse ? tgl.accent : Theme.buttonNormalColor)
        color: tgl.on
                ? tgl.accent
                : (tglMouse.pressed
                        ? tgl.accent
                        : (tglMouse.containsMouse ? Theme.darkGray2 : "transparent"))

        Text {
            anchors.centerIn: parent
            text: tgl.glyph
            font.pixelSize: 10
            font.bold: true
            color: tgl.on ? Theme.knobBackgroundColor : Theme.deckTextColor
        }

        MouseArea {
            id: tglMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: tgl.toggled()
        }
    }

    Skin.SectionBackground {
        anchors.fill: parent
    }

    // Minimum width a stem cell needs so its controls never overlap. Cells
    // fill the strip evenly when there's room, but never shrink below this;
    // clip keeps any overflow (very narrow windows) from bleeding out.
    readonly property real minCellWidth: 300

    Row {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5
        clip: true

        Repeater {
            model: root.player.stemsModel

            Item {
                id: cell

                required property int index
                required property string label
                required property color color

                readonly property string sGroup: root.stemGroup(cell.index)
                readonly property string fxGroup: `[QuickEffectRack1_${cell.sGroup}]`

                width: Math.max(root.minCellWidth, (root.width - 10 - 3 * 5) / 4)
                height: parent.height

                Mixxx.ControlProxy {
                    id: muteControl

                    group: cell.sGroup
                    key: "mute"
                }

                Mixxx.ControlProxy {
                    id: fxEnabledControl

                    group: cell.fxGroup
                    key: "enabled"
                }

                Mixxx.ControlProxy {
                    id: fxPresetControl

                    group: cell.fxGroup
                    key: "loaded_chain_preset"
                }

                // Apply solo to this stem's mute whenever the soloed stem changes.
                Connections {
                    target: root
                    function onSoloedStemChanged() {
                        muteControl.value = (root.soloedStem === -1)
                                ? 0
                                : (cell.index === root.soloedStem ? 0 : 1);
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: Qt.rgba(1, 1, 1, 0.03)
                    border.width: 1
                    border.color: Qt.rgba(cell.color.r, cell.color.g, cell.color.b, 0.55)
                }

                Row {
                    anchors.fill: parent
                    anchors.margins: 4
                    anchors.leftMargin: 12
                    spacing: 6

                    // color chip + label
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 2
                            color: cell.color
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: cell.label
                            color: Theme.white
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                            width: 46
                        }
                    }

                    StemToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: "M"
                        accent: Theme.red
                        on: muteControl.value > 0
                        onToggled: muteControl.value = !muteControl.value
                    }

                    StemToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: "S"
                        accent: Theme.yellow
                        on: root.soloedStem === cell.index
                        onToggled: root.toggleSolo(cell.index)
                    }

                    // volume knob
                    Skin.ControlKnob {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 34
                        height: 34
                        arcStart: 0
                        group: cell.sGroup
                        key: "volume"
                        color: Theme.gainKnobColor
                    }

                    // FX: enable + amount + preset
                    StemToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: "FX"
                        width: 26
                        accent: Theme.effectColor
                        on: fxEnabledControl.value > 0
                        onToggled: fxEnabledControl.value = !fxEnabledControl.value
                    }

                    Skin.ControlMiniKnob {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 22
                        height: 22
                        arcStart: Knob.ArcStart.Minimum
                        group: cell.fxGroup
                        key: "super1"
                        color: Theme.effectColor
                        opacity: fxEnabledControl.value ? 1 : 0.5
                    }

                    ComboBox {
                        id: fxPreset

                        anchors.verticalCenter: parent.verticalCenter
                        // Snug — just wide enough for a preset name + chevron.
                        width: 96
                        height: 22
                        opacity: fxEnabledControl.value ? 1 : 0.6
                        textRole: "display"
                        font.pixelSize: 10
                        model: Mixxx.EffectsManager.quickChainPresetModel
                        currentIndex: fxPresetControl.value === -1 ? 0 : fxPresetControl.value
                        onActivated: (index) => {
                            fxPresetControl.value = index;
                        }

                        background: Rectangle {
                            color: Theme.knobBackgroundColor
                            radius: 3
                            border.width: 1
                            border.color: (fxPreset.pressed || fxPreset.popup.visible)
                                    ? Theme.blue
                                    : (fxPreset.hovered ? Theme.deckTextColor : Theme.midGray)
                        }

                        contentItem: Text {
                            leftPadding: 5
                            rightPadding: 14
                            text: fxPreset.displayText
                            color: Theme.deckTextColor
                            font: fxPreset.font
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        indicator: Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: "▾"
                            font.pixelSize: 8
                            color: Theme.deckTextColor
                        }

                        delegate: ItemDelegate {
                            id: presetItem

                            required property int index
                            width: ListView.view ? ListView.view.width : fxPreset.width
                            highlighted: fxPreset.highlightedIndex === presetItem.index

                            contentItem: Text {
                                text: fxPreset.textAt(presetItem.index)
                                color: Theme.deckTextColor
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: presetItem.highlighted ? Qt.rgba(0.004, 0.863, 0.988, 0.18) : "transparent"
                            }
                        }

                        popup: Popup {
                            y: fxPreset.height + 2
                            width: Math.max(fxPreset.width, 160)
                            implicitHeight: Math.min(contentItem.implicitHeight, 320)
                            padding: 4

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: fxPreset.popup.visible ? fxPreset.delegateModel : null
                                currentIndex: fxPreset.highlightedIndex
                                ScrollIndicator.vertical: ScrollIndicator {}
                            }
                            background: Rectangle {
                                color: Theme.darkGray2
                                radius: 4
                                border.color: Theme.midGray
                                border.width: 1
                            }
                        }
                    }
                }
            }
        }
    }
}
