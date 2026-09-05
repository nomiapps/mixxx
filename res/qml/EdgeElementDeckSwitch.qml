import "." as Skin
import QtQuick 2.12
import "Theme"

// Hardware-style deck select.
// Single-slot: spec.slot + spec.options cycles one side (e.g. DECK 1 -> 3).
// Pair mode: spec.slots = { "@left": ["[Channel1]", "[Channel3]"],
//                           "@right": ["[Channel2]", "[Channel4]"] }
// cycles every listed slot in lockstep (index-synced) - one tap flips the
// whole surface between deck pairs, like toggling 1/2 <-> 3/4.
// Lights up when off the primary (first) option.
Item {
    id: root

    readonly property int currentIndex: {
        if (!surface)
            return 0;

        const current = surface.deckAssign[slotNames[0]] ?? firstOptions[0];
        const i = firstOptions.indexOf(current);
        return i < 0 ? 0 : i;
    }
    readonly property var firstOptions: pairMode ? spec.slots[slotNames[0]] : (spec.options ?? ["[Channel1]", "[Channel3]"])
    readonly property string labelText: {
        if (!pairMode)
            return "DECK " + deckNumber(surface ? (surface.deckAssign[spec.slot] ?? firstOptions[0]) : firstOptions[0]);

        const nums = slotNames.map(s => {
            const opts = spec.slots[s];
            return deckNumber(opts[Math.min(currentIndex, opts.length - 1)]);
        });
        return "DECKS " + nums.join("/");
    }
    readonly property bool pairMode: !!spec.slots
    readonly property var slotNames: pairMode ? Object.keys(spec.slots) : [spec.slot]
    required property var spec
    property var surface: null

    function deckNumber(group) {
        const m = /\[Channel(\d+)\]/.exec(group);
        return m ? m[1] : "?";
    }

    Skin.Button {
        activeColor: Theme.deckActiveColor
        anchors.fill: parent
        highlight: root.currentIndex > 0
        text: root.labelText

        onClicked: {
            if (!root.surface)
                return;

            const next = (root.currentIndex + 1) % root.firstOptions.length;
            for (const slot of root.slotNames) {
                const opts = root.pairMode ? root.spec.slots[slot] : root.firstOptions;
                root.surface.setDeck(slot, opts[Math.min(next, opts.length - 1)]);
            }
        }
    }
}
