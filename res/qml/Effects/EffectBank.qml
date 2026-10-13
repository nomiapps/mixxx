import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

// The effects the way a controller has them: six buttons in the middle, three
// to a unit, with a switch at each side that sends that side's deck through
// them, one dry/wet knob for the lot, and a readout of what the BEATS encoder
// is set to. Both decks share the six.
Item {
    id: root

    readonly property var buttons: [unit1Effect1, unit1Effect2, unit1Effect3, unit2Effect1, unit2Effect2, unit2Effect3]
    readonly property int buttonHeight: 30
    readonly property int buttonWidth: 104
    readonly property int deckSwitchWidth: 64
    readonly property int gap: 5
    // The first effect that is picked, or null: the one the BEATS readout is about.
    readonly property var pickedButton: {
        for (const button of root.buttons) {
            if (button.picked)
                return button;
        }
        return null;
    }
    // Whether the choosers and knobs behind the buttons are open.
    property bool rackShown: false
    property bool show4decks: false
    property bool showFourUnits: false

    // As on the controller: a press picks that effect and drops the others, and
    // a press on the one that is picked drops it too. Several at once are still
    // a matter of the ON buttons in the rack.
    function press(button) {
        const wasPicked = button.picked;
        for (const other of root.buttons)
            other.setPicked(false);
        button.setPicked(!wasPicked);
    }

    implicitHeight: bank.height + 2 * root.gap

    // The six buttons sit in the middle, under the mixer; everything else is
    // hung off them, so nothing beside them can push them off centre.
    Item {
        id: bank

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 2 * root.buttonHeight + root.gap

        EffectBeatsReadout {
            anchors.right: leftDecks.left
            anchors.rightMargin: 2 * root.gap
            controlKey: root.pickedButton ? root.pickedButton.encoderKey : ""
            height: parent.height
            slot: root.pickedButton ? root.pickedButton.slot : null
            width: 110
        }
        Column {
            id: leftDecks

            anchors.right: grid.left
            anchors.rightMargin: 2 * root.gap
            height: parent.height
            spacing: root.gap
            width: root.deckSwitchWidth

            EffectDeckSwitch {
                deckNumber: 1
                height: root.show4decks ? (parent.height - parent.spacing) / 2 : parent.height
                showFourUnits: root.showFourUnits
                width: parent.width
            }
            EffectDeckSwitch {
                deckNumber: 3
                height: (parent.height - parent.spacing) / 2
                showFourUnits: root.showFourUnits
                visible: root.show4decks
                width: parent.width
            }
        }
        Grid {
            id: grid

            anchors.horizontalCenter: parent.horizontalCenter
            columnSpacing: root.gap
            columns: 3
            rowSpacing: root.gap

            EffectBankButton {
                id: unit1Effect1

                effectNumber: 1
                height: root.buttonHeight
                unitNumber: 1
                width: root.buttonWidth

                onClicked: root.press(unit1Effect1)
            }
            EffectBankButton {
                id: unit1Effect2

                effectNumber: 2
                height: root.buttonHeight
                unitNumber: 1
                width: root.buttonWidth

                onClicked: root.press(unit1Effect2)
            }
            EffectBankButton {
                id: unit1Effect3

                effectNumber: 3
                height: root.buttonHeight
                unitNumber: 1
                width: root.buttonWidth

                onClicked: root.press(unit1Effect3)
            }
            EffectBankButton {
                id: unit2Effect1

                effectNumber: 1
                height: root.buttonHeight
                unitNumber: 2
                width: root.buttonWidth

                onClicked: root.press(unit2Effect1)
            }
            EffectBankButton {
                id: unit2Effect2

                effectNumber: 2
                height: root.buttonHeight
                unitNumber: 2
                width: root.buttonWidth

                onClicked: root.press(unit2Effect2)
            }
            EffectBankButton {
                id: unit2Effect3

                effectNumber: 3
                height: root.buttonHeight
                unitNumber: 2
                width: root.buttonWidth

                onClicked: root.press(unit2Effect3)
            }
        }
        Column {
            id: rightDecks

            anchors.left: grid.right
            anchors.leftMargin: 2 * root.gap
            height: parent.height
            spacing: root.gap
            width: root.deckSwitchWidth

            EffectDeckSwitch {
                deckNumber: 2
                height: root.show4decks ? (parent.height - parent.spacing) / 2 : parent.height
                showFourUnits: root.showFourUnits
                width: parent.width
            }
            EffectDeckSwitch {
                deckNumber: 4
                height: (parent.height - parent.spacing) / 2
                showFourUnits: root.showFourUnits
                visible: root.show4decks
                width: parent.width
            }
        }
        Item {
            id: mix

            anchors.left: rightDecks.right
            anchors.leftMargin: 2 * root.gap
            height: parent.height
            width: 44

            Skin.ControlKnob {
                id: mixKnob

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                arcStart: Skin.Knob.ArcStart.Minimum
                color: Theme.effectUnitColor
                group: "[EffectRack1_EffectUnit1]"
                height: 44
                key: "mix"
                width: 44
            }
            Skin.EmbeddedText {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                font.bold: false
                text: "DRY/WET"
            }
        }
        Skin.Button {
            accessibleName: root.rackShown ? "Hide the effect rack" : "Show the effect rack"
            activeColor: Theme.effectUnitColor
            anchors.left: mix.right
            anchors.leftMargin: 2 * root.gap
            anchors.verticalCenter: parent.verticalCenter
            height: root.buttonHeight
            highlight: root.rackShown
            text: root.rackShown ? "▲" : "▼"
            width: 36

            onClicked: root.rackShown = !root.rackShown
        }
    }
    // One knob for the bank, as on a controller: the second unit follows the first.
    Mixxx.ControlProxy {
        id: unit1Mix

        group: "[EffectRack1_EffectUnit1]"
        key: "mix"

        onValueChanged: {
            if (Math.abs(unit2Mix.value - unit1Mix.value) > 1e-9)
                unit2Mix.value = unit1Mix.value;
        }
    }
    Mixxx.ControlProxy {
        id: unit2Mix

        group: "[EffectRack1_EffectUnit2]"
        key: "mix"
    }
}
