import "." as Skin
import QtQuick 2.12
import "Theme"

// A layout-local switch. Unlike every other button on the surface it writes no
// Mixxx control at all: it flips a named flag on the surface, and the layout
// reacts. An element carrying "showIf": "<flag>" exists only while the flag is
// on, and one carrying "rectIf": {"<flag>": [x, y, w, h]} uses that rect
// instead of its own while it is -- enough for a section to appear and for its
// neighbour to give up the space, without the engine knowing what a waveform is.
Item {
    id: root

    readonly property string flag: root.spec.flag ?? ""
    required property var spec
    property var surface: null

    Skin.Button {
        activeColor: root.spec.color ?? Theme.deckActiveColor
        anchors.fill: parent
        highlight: root.surface ? root.surface.toggleOn(root.flag) : false
        text: root.spec.label ?? ""

        onClicked: {
            if (root.surface)
                root.surface.setToggle(root.flag, !root.surface.toggleOn(root.flag));
        }
    }
}
