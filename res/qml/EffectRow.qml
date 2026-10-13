import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "Effects" as Effects

// The effects as a controller lays them out: one bank in the middle, a row of
// three for each unit, and a switch for each deck at the side that deck is on.
// The units used to sit side by side, FX 1 under the left deck and FX 2 under
// the right, which read as "the left deck's effects" and "the right deck's".
// They are not: every unit can play on any deck.
Item {
    id: root

    readonly property int deckSwitchWidth: 64
    property bool show4decks: false
    readonly property bool showFourUnits: showFourUnitsControl.value > 0

    implicitHeight: units.height

    Column {
        id: leftDecks

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 5
        anchors.top: parent.top
        spacing: 5
        width: root.deckSwitchWidth

        Effects.EffectDeckSwitch {
            deckNumber: 1
            height: root.show4decks ? (parent.height - parent.spacing) / 2 : parent.height
            showFourUnits: root.showFourUnits
            width: parent.width
        }
        Effects.EffectDeckSwitch {
            deckNumber: 3
            height: (parent.height - parent.spacing) / 2
            showFourUnits: root.showFourUnits
            visible: root.show4decks
            width: parent.width
        }
    }
    Column {
        id: units

        anchors.left: leftDecks.right
        anchors.right: rightDecks.left

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
    Column {
        id: rightDecks

        anchors.bottom: parent.bottom
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 5
        width: root.deckSwitchWidth

        Effects.EffectDeckSwitch {
            deckNumber: 2
            height: root.show4decks ? (parent.height - parent.spacing) / 2 : parent.height
            showFourUnits: root.showFourUnits
            width: parent.width
        }
        Effects.EffectDeckSwitch {
            deckNumber: 4
            height: (parent.height - parent.spacing) / 2
            showFourUnits: root.showFourUnits
            visible: root.show4decks
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
