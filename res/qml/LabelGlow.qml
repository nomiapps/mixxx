import QtQuick

// A soft glow behind a Text, without a shader: faint copies of the text spread
// round it on three rings, fainter the further out, which the eye reads as a
// halo. Put it inside the Text
// it glows for, filling it, on a Loader that is active only while the control
// is lit: an unlit button then costs nothing.
//
// It stands in for the Glow effect the buttons used to carry. Every one of
// those, lit or not, was a shader item that Qt rebuilt on the GUI thread when
// the window moved to a screen with another scale factor.
Item {
    id: root

    required property Text target
    property color color: target.color
    // How far the glow reaches from the glyph edges, in pixels.
    property real radius: 4
    // Each ring: how many copies, how far out as a fraction of the radius, and
    // how opaque each copy is.
    readonly property var rings: [
        {
            "copies": 8,
            "reach": 0.35,
            "strength": 0.035
        },
        {
            "copies": 12,
            "reach": 0.6,
            "strength": 0.022
        },
        {
            "copies": 16,
            "reach": 0.85,
            "strength": 0.013
        }
    ]
    readonly property var copies: {
        const list = [];
        for (const ring of root.rings) {
            for (let i = 0; i < ring.copies; ++i) {
                const angle = 2 * Math.PI * i / ring.copies;
                list.push({
                    "dx": Math.cos(angle) * root.radius * ring.reach,
                    "dy": Math.sin(angle) * root.radius * ring.reach,
                    "strength": ring.strength
                });
            }
        }
        return list;
    }

    // Behind the text it belongs to.
    z: -1

    Repeater {
        model: root.copies

        Text {
            required property var modelData

            color: root.color
            elide: root.target.elide
            font: root.target.font
            height: root.height
            horizontalAlignment: root.target.horizontalAlignment
            opacity: modelData.strength
            text: root.target.text
            verticalAlignment: root.target.verticalAlignment
            width: root.width
            x: modelData.dx
            y: modelData.dy
        }
    }
}
