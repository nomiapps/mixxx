pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.12
import "Theme"

// The built-in synth on the main window, as a row alongside the samplers and
// the mic/aux inputs.
//
// The synth is an engine channel like any other, so it has always been audible
// and mappable; it just had nowhere to play it from unless the Edge surface was
// up. This hosts the same keyboard panel that surface uses, which keeps one
// implementation of the key handling and the waveform and envelope controls.
//
// The panel is built for touch, so it is given a generous height here rather
// than squeezed: on a mouse it is still played one note at a time, and on a
// touchscreen the whole row is playable.
Item {
    id: root

    // Which synth channel this row plays. There is one today; the property is
    // here so a second does not need a new file.
    property string group: "[Synth1]"
    property int octaves: 3

    implicitHeight: 150

    Skin.SectionBackground {
        anchors.fill: parent
    }
    Skin.EdgeElementSynth {
        anchors.fill: parent
        anchors.margins: 5

        // The Edge surface passes itself so an element can resolve a relative
        // deck ("[Channel1]" vs "the left deck"). There is nothing relative
        // here, so the group in the spec is used as it stands.
        spec: ({
                "group": root.group,
                "octaves": root.octaves
            })
    }
}
