import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

// Sends one deck through the effect units, or takes it out again: the paddle
// a controller has for each deck beside its bank of effect buttons. Which
// effects are heard is chosen in the units; this only says for which deck.
Skin.Button {
    id: root

    required property int deckNumber
    readonly property string enableKey: "group_[Channel" + root.deckNumber + "]_enable"
    // Lit when any unit has the deck: the units' own routing can also be set
    // one at a time elsewhere, and this must not claim the deck is dry then.
    readonly property bool on: unit1.value > 0 || unit2.value > 0 || (root.showFourUnits && (unit3.value > 0 || unit4.value > 0))
    property bool showFourUnits: false

    accessibleName: "Deck " + root.deckNumber + " effects"
    activeColor: Theme.effectUnitColor
    highlight: root.on
    text: "DECK " + root.deckNumber + "\nFX"

    onClicked: {
        const value = root.on ? 0 : 1;
        unit1.value = value;
        unit2.value = value;
        if (root.showFourUnits) {
            unit3.value = value;
            unit4.value = value;
        }
    }

    Mixxx.ControlProxy {
        id: unit1

        group: "[EffectRack1_EffectUnit1]"
        key: root.enableKey
    }
    Mixxx.ControlProxy {
        id: unit2

        group: "[EffectRack1_EffectUnit2]"
        key: root.enableKey
    }
    Mixxx.ControlProxy {
        id: unit3

        group: "[EffectRack1_EffectUnit3]"
        key: root.enableKey
    }
    Mixxx.ControlProxy {
        id: unit4

        group: "[EffectRack1_EffectUnit4]"
        key: root.enableKey
    }
}
