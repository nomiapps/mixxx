import "." as Skin
import QtQuick 2.12
import "Theme"

// Hardware-style deck select: cycles a deck slot (spec.slot, e.g. "@left")
// through spec.options (e.g. ["[Channel1]", "[Channel3]"]). Everything in the
// layout bound to that slot retargets instantly. Lights up when the slot is
// on any option other than the first, like the DECK 3/4 LEDs on controllers.
Item {
    id: root

    required property var spec
    property var surface: null

    readonly property var options: spec.options ?? ["[Channel1]", "[Channel3]"]
    readonly property string current: surface ? (surface.deckAssign[spec.slot] ?? options[0]) : options[0]

    function deckNumber(group) {
        const m = /\[Channel(\d+)\]/.exec(group);
        return m ? m[1] : "?";
    }

    Skin.Button {
        anchors.fill: parent
        text: "DECK " + root.deckNumber(root.current)
        activeColor: Theme.deckActiveColor
        highlight: root.options.indexOf(root.current) > 0
        onClicked: {
            if (!root.surface)
                return ;

            const i = root.options.indexOf(root.current);
            root.surface.setDeck(root.spec.slot, root.options[(i + 1) % root.options.length]);
        }
    }
}
