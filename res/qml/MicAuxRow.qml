pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.12

// Microphone and auxiliary inputs, laid out like the deck rows above it:
// ducking plus the mics on the left, the auxiliaries on the right.
//
// Each unit gates itself on its own "input_configured" control, so an input
// that is not set up in the sound hardware preferences shows a compact
// placeholder rather than a full strip. The row is therefore always the same
// height whether or not anything is configured.
Item {
    id: root

    property int auxCount: 4
    property int micCount: 4

    implicitHeight: Math.max(micRow.implicitHeight, auxRow.implicitHeight)

    Skin.SectionBackground {
        anchors.fill: parent
    }
    Row {
        id: micRow

        padding: 5
        spacing: 10

        anchors {
            bottom: parent.bottom
            left: parent.left
            top: parent.top
        }
        Skin.MicrophoneDuckingPanel {
        }
        Repeater {
            model: Math.max(0, root.micCount)

            Skin.MicrophoneUnit {
                required property int index

                unitNumber: index + 1
            }
        }
    }
    Row {
        id: auxRow

        padding: 5
        spacing: 10

        anchors {
            bottom: parent.bottom
            right: parent.right
            top: parent.top
        }
        Repeater {
            model: Math.max(0, root.auxCount)

            Skin.AuxiliaryUnit {
                required property int index

                unitNumber: index + 1
            }
        }
    }
}
