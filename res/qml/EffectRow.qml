import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "Effects" as Effects

// The effects as a controller has them: six buttons in the middle with a
// switch for each deck beside them. The rack behind the buttons -- which
// effect is on each one, and its knobs -- opens underneath when it is wanted,
// and is otherwise out of the way.
Item {
    id: root

    property bool show4decks: false
    readonly property bool showFourUnits: showFourUnitsControl.value > 0

    implicitHeight: bank.height + (bank.rackShown ? rack.height : 0)

    Effects.EffectBank {
        id: bank

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: implicitHeight
        show4decks: root.show4decks
        showFourUnits: root.showFourUnits
    }
    Column {
        id: rack

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: bank.bottom
        visible: bank.rackShown

        Effects.EffectUnit {
            unitNumber: 1
            width: parent.width
        }
        Effects.EffectUnit {
            unitNumber: 2
            width: parent.width
        }
        Effects.EffectUnit {
            unitNumber: 3
            visible: root.showFourUnits
            width: parent.width
        }
        Effects.EffectUnit {
            unitNumber: 4
            visible: root.showFourUnits
            width: parent.width
        }
    }
    Skin.SectionBackground {
        anchors.fill: parent
        z: -1
    }
    Mixxx.ControlProxy {
        id: showFourUnitsControl

        group: "[Skin]"
        key: "show_4effectunits"
    }
}
