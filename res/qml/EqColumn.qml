import "." as Skin
import QtQuick 2.12
import QtQuick.Shapes 1.12
import "Theme"

Item {
    id: root

    required property string group

    implicitHeight: controls.implicitHeight
    implicitWidth: 42

    Rectangle {
        anchors.fill: parent
        border.color: "#343438"
        border.width: 1
        color: "#171719"
        radius: 5
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.accentColor
        height: 2
        radius: 1
        z: 2
    }
    Column {
        id: controls

        spacing: 4
        width: parent.width

        Skin.EqKnob {
            knob.color: Theme.eqHighColor
            knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
            knob.key: "parameter3"
            labelText: qsTr("HIGH")
            statusKey: "button_parameter3"
        }
        Skin.EqKnob {
            knob.color: Theme.eqMidColor
            knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
            knob.key: "parameter2"
            labelText: qsTr("MID")
            statusKey: "button_parameter2"
        }
        Skin.EqKnob {
            knob.color: Theme.eqLowColor
            knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
            knob.key: "parameter1"
            labelText: qsTr("LOW")
            statusKey: "button_parameter1"
        }
        Skin.QuickFxKnob {
            group: "[QuickEffectRack1_" + root.group + "]"
            knob.arcStyle: ShapePath.DashLine
            knob.arcStylePattern: [2, 2]
            knob.color: Theme.eqFxColor
        }
    }
}
