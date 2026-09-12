pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.12
import "Theme"

// The step sequencer on the main window, as a row alongside the samplers,
// the mic/aux inputs and the synth.
//
// It hosts the same panel the Edge surface uses, in its compact form: the
// transport and the selected step's detail row at fixed heights, smaller
// type, and two of the four sampler lanes so the cells stay tall enough to
// hit with a mouse. Everything is a control, so the Edge panel, a controller
// mapping and this row all edit the one pattern.
Item {
    id: root

    // Which sequencer this row edits. There is one today; the property is
    // here so a second does not need a new file.
    property string group: "[Sequencer1]"
    property int samplerCount: 8
    property int samplerLanes: 2

    implicitHeight: 220

    Skin.SectionBackground {
        anchors.fill: parent
    }
    Skin.EdgeElementSequencer {
        anchors.fill: parent
        anchors.margins: 5

        // No surface: nothing here is relative to a deck slot, so the group
        // in the spec is used as it stands.
        spec: ({
                "group": root.group,
                "samplerLanes": root.samplerLanes,
                "samplerCount": root.samplerCount,
                "compact": true
            })
    }
}
