pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "Theme"

// A dedicated, full-width strip of per-stem controls (label, Mute, Solo,
// volume, quick-effect enable/amount/preset) that appears under a deck's
// waveform when a stem track is loaded. Separate from -- and in addition to --
// the standard channel EQ.
Item {
    id: root

    // Whether the host wants the strip shown at all (e.g. hidden when the
    // library is maximized). Combined with the actual stem count.
    property bool active: true
    required property string group
    readonly property bool hasStems: stemCountControl.value > 0
    // Minimum width a stem cell needs so its controls never overlap. Cells
    // fill the strip evenly when there's room, but never shrink below this;
    // clip keeps any overflow (very narrow windows) from bleeding out.
    readonly property real minCellWidth: 300
    readonly property var player: Mixxx.PlayerManager.getPlayer(root.group)
    // Index of the currently soloed stem, or -1 for none.
    property int soloedStem: -1
    // The stems model lives on the loaded track, so re-resolve it per track.
    readonly property var stemsModel: root.hasStems && root.player.currentTrack ? root.player.currentTrack.stemsModel : []

    function stemGroup(index) {
        return `${root.group.substr(0, root.group.length - 1)}_Stem${index + 1}]`;
    }
    // Solo = exclusive mute: soloing a stem mutes the other three; clicking the
    // lit solo again clears it and unmutes all. Each cell applies this to its
    // own mute control when soloedStem changes.
    function toggleSolo(index) {
        root.soloedStem = (root.soloedStem === index) ? -1 : index;
    }

    implicitHeight: visible ? 52 : 0
    visible: root.active && root.hasStems && Mixxx.Config.waveformShowStemStrips
    z: 1

    // A solo must not outlive the track it was set on.
    onStemsModelChanged: root.soloedStem = -1

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }
    Skin.SectionBackground {
        anchors.fill: parent
    }
    Row {
        anchors.fill: parent
        anchors.margins: 5
        clip: true
        spacing: 5

        Repeater {
            model: root.stemsModel

            Item {
                id: cell

                required property color color
                // The Repeater sets index to -1 on delegates it is tearing down
                // (the stems model is swapped on every track load). Every
                // binding on index re-evaluates then, so without this guard the
                // group briefly becomes "[ChannelN_Stem0]" and each ControlProxy
                // below chases a CO that does not exist. Freeze the last valid
                // group instead; the item is destroyed moments later anyway.
                property string frozenStemGroup: ""
                readonly property string fxGroup: `[QuickEffectRack1_${cell.stemGroup}]`
                required property int index
                required property string label
                readonly property string stemGroup: cell.index >= 0 ? root.stemGroup(cell.index) : cell.frozenStemGroup

                height: parent.height
                width: Math.max(root.minCellWidth, (root.width - 10 - 3 * 5) / 4)

                Component.onCompleted: cell.frozenStemGroup = cell.stemGroup
                onStemGroupChanged: {
                    if (cell.index >= 0)
                        cell.frozenStemGroup = cell.stemGroup;
                }

                Mixxx.ControlProxy {
                    id: muteControl

                    group: cell.stemGroup
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
                    function onSoloedStemChanged() {
                        muteControl.value = (root.soloedStem === -1) ? 0 : (cell.index === root.soloedStem ? 0 : 1);
                    }

                    target: root
                }
                Rectangle {
                    anchors.fill: parent
                    border.color: Qt.rgba(cell.color.r, cell.color.g, cell.color.b, 0.55)
                    border.width: 1
                    color: Qt.rgba(1, 1, 1, 0.03)
                    radius: 4
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.margins: 4
                    spacing: 6

                    // color chip + label
                    Rectangle {
                        Layout.preferredHeight: 8
                        Layout.preferredWidth: 8
                        color: cell.color
                        radius: 2
                    }
                    Text {
                        Layout.preferredWidth: 46
                        color: muteControl.value > 0 ? Theme.midGray : Theme.deckTextColor
                        elide: Text.ElideRight
                        font.bold: true
                        font.capitalization: Font.AllUppercase
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.buttonFontPixelSize
                        text: cell.label
                    }
                    StemToggle {
                        accent: Theme.red
                        glyph: "M"
                        on: muteControl.value > 0

                        onToggled: muteControl.value = !muteControl.value
                    }
                    StemToggle {
                        accent: Theme.yellow
                        glyph: "S"
                        on: root.soloedStem === cell.index

                        onToggled: root.toggleSolo(cell.index)
                    }
                    Skin.ControlKnob {
                        Layout.preferredHeight: 34
                        Layout.preferredWidth: 34
                        arcStart: Knob.ArcStart.Minimum
                        color: Theme.gainKnobColor
                        group: cell.stemGroup
                        key: "volume"
                    }
                    // FX: enable + amount + preset
                    StemToggle {
                        Layout.preferredWidth: 26
                        accent: Theme.effectColor
                        glyph: "FX"
                        on: fxEnabledControl.value > 0

                        onToggled: fxEnabledControl.value = !fxEnabledControl.value
                    }
                    Skin.ControlMiniKnob {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: 22
                        arcStart: Knob.ArcStart.Minimum
                        color: Theme.effectColor
                        group: cell.fxGroup
                        key: "super1"
                        opacity: fxEnabledControl.value ? 1 : 0.5
                    }
                    Skin.OpaqueComboBox {
                        id: fxPreset

                        // Takes whatever width is left in the cell, within reason.
                        Layout.fillWidth: true
                        Layout.maximumWidth: 180
                        Layout.minimumWidth: 80
                        Layout.preferredHeight: 22
                        clip: true
                        currentIndex: fxPresetControl.value === -1 ? 0 : fxPresetControl.value
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        model: Mixxx.EffectsManager.quickChainPresetModel
                        opacity: fxEnabledControl.value ? 1 : 0.6
                        popupWidth: 160
                        textRole: "display"

                        onActivated: index => {
                            fxPresetControl.value = index;
                        }
                    }
                }
            }
        }
    }

    // Small labelled toggle button used for Mute / Solo / FX-enable.
    component StemToggle: Rectangle {
        id: tgl

        property color accent: Theme.red
        property string glyph: ""
        property bool on: false

        signal toggled

        border.color: tgl.on || tglMouse.containsMouse ? tgl.accent : Theme.buttonNormalColor
        border.width: 1
        color: tgl.on || tglMouse.pressed ? tgl.accent : (tglMouse.containsMouse ? Theme.darkGray2 : "transparent")
        implicitHeight: 20
        implicitWidth: 22
        radius: 3

        Text {
            anchors.centerIn: parent
            color: tgl.on ? Theme.knobBackgroundColor : Theme.deckTextColor
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.buttonFontPixelSize
            text: tgl.glyph
        }
        MouseArea {
            id: tglMouse

            anchors.fill: parent
            hoverEnabled: true

            onClicked: tgl.toggled()
        }
    }
}
