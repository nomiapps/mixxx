import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

Item {
    id: root

    readonly property bool expanded: expandedControl.value > 0
    readonly property string group: unit.group
    readonly property Mixxx.EffectUnitProxy unit: Mixxx.EffectsManager.getEffectUnit(unitNumber)
    required property int unitNumber

    // Tall enough for BOTH sides: the effect slots, and the frame of unit
    // controls beside them with its 5 px margin above and below.
    implicitHeight: Math.max(effectContainer.height, effectUnitControlsFrame.height + 10)

    Item {
        id: effectContainer

        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: effectUnitControlsFrame.left
        anchors.top: parent.top
        height: root.expanded ? 160 : 60

        EffectSlot {
            id: effect1

            anchors.left: parent.left
            anchors.top: parent.top
            effectNumber: 1
            expanded: root.expanded
            height: 50
            unitNumber: root.unitNumber
            width: root.expanded ? parent.width : parent.width / 3
        }
        EffectSlot {
            id: effect2

            anchors.left: root.expanded ? parent.left : effect1.right
            anchors.top: root.expanded ? effect1.bottom : parent.top
            effectNumber: 2
            expanded: root.expanded
            height: 50
            unitNumber: root.unitNumber
            width: root.expanded ? parent.width : parent.width / 3
        }
        EffectSlot {
            id: effect3

            anchors.left: root.expanded ? parent.left : effect2.right
            anchors.top: root.expanded ? effect2.bottom : parent.top
            effectNumber: 3
            expanded: root.expanded
            height: 50
            unitNumber: root.unitNumber
            width: root.expanded ? parent.width : parent.width / 3
        }
    }
    Rectangle {
        id: effectUnitControlsFrame

        // One line of controls, as tall as a row of effect slots: the units
        // are stacked, so a unit that is two lines tall costs the library
        // twice over.
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.knobBackgroundColor
        height: 50
        radius: 5
        width: unitControls.implicitWidth + 10

        Row {
            id: unitControls

            anchors.centerIn: parent
            spacing: 5

            Skin.ControlButton {
                activeColor: Theme.effectUnitColor
                anchors.verticalCenter: parent.verticalCenter
                group: root.group
                height: 26
                key: "show_parameters"
                text: root.expanded ? "▲" : "▼"
                toggleable: true
                width: 36
            }
            Skin.Button {
                activeColor: Theme.effectUnitColor
                anchors.verticalCenter: parent.verticalCenter
                height: 26
                text: "FX " + root.unitNumber
                width: 56

                onClicked: presetPopup.open()
            }
            Skin.ControlButton {
                activeColor: Theme.effectUnitColor
                anchors.verticalCenter: parent.verticalCenter
                group: root.group
                height: 26
                key: "group_[Headphone]_enable"
                text: "PFL"
                toggleable: true
                width: 42
            }
            Skin.ControlButton {
                activeColor: Theme.effectUnitColor
                anchors.verticalCenter: parent.verticalCenter
                group: root.group
                height: 26
                key: "mix_mode"
                text: "D/W"
                toggleable: true
                width: 36
            }
            Skin.ControlKnob {
                arcStart: Skin.Knob.ArcStart.Minimum
                color: Theme.effectUnitColor
                group: root.group
                height: 40
                key: "mix"
                width: 40
            }
            Skin.ControlKnob {
                arcStart: Skin.Knob.ArcStart.Minimum
                color: Theme.effectUnitColor
                group: root.group
                height: 40
                key: "super1"
                visible: showSuperKnobsControl.value > 0
                width: visible ? 40 : 0
            }
        }
    }
    EffectChainPresetPopup {
        id: presetPopup

        slot1: effect1.slot
        slot2: effect2.slot
        slot3: effect3.slot
        unit: root.unit
    }
    Mixxx.ControlProxy {
        id: expandedControl

        group: root.group
        key: "show_parameters"
    }
    Mixxx.ControlProxy {
        id: showSuperKnobsControl

        group: "[Skin]"
        key: "show_superknobs"
    }
}
