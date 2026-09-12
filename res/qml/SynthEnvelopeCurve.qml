import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Shapes
import "Theme"

// A synth's amplitude envelope as the shape it will have: attack up to full,
// decay down to the sustain level, a held stretch, then the release. Each
// stage's width is the square root of its seconds, so a short attack next to
// a long release still shows as a slope rather than a wall, using the
// engine's own knob-to-seconds mapping.
Rectangle {
    id: root

    required property string group
    property color lineColor: Theme.green
    readonly property var points: root.buildPoints(root.width, root.height, attack.value, decay.value, sustain.value, release.value)

    function buildPoints(w, h, a, d, s, r) {
        const inset = 4;
        const top = inset;
        const bottom = h - inset;
        const level = Math.max(0, Math.min(1, s));
        const ta = Math.sqrt(Mixxx.Synth.envelopeSeconds(a, 0));
        const td = Math.sqrt(Mixxx.Synth.envelopeSeconds(d, 1));
        const tr = Math.sqrt(Mixxx.Synth.envelopeSeconds(r, 2));
        // The held stretch is a fixed quarter of the drawing, so the eye has
        // a plateau to read the sustain level from.
        const hold = 0.25;
        const scale = (w - 2 * inset) * (1 - hold) / Math.max(1e-6, ta + td + tr);
        let x = inset;
        const pts = [Qt.point(x, bottom)];
        x += ta * scale;
        pts.push(Qt.point(x, top));
        x += td * scale;
        pts.push(Qt.point(x, bottom - level * (bottom - top)));
        x += (w - 2 * inset) * hold;
        pts.push(Qt.point(x, bottom - level * (bottom - top)));
        x += tr * scale;
        pts.push(Qt.point(x, bottom));
        return pts;
    }

    border.color: Theme.panelBorderColor
    color: Theme.sunkenBackgroundColor
    radius: 3

    Mixxx.ControlProxy {
        id: attack

        group: root.group
        key: "attack"
    }
    Mixxx.ControlProxy {
        id: decay

        group: root.group
        key: "decay"
    }
    Mixxx.ControlProxy {
        id: sustain

        group: root.group
        key: "sustain"
    }
    Mixxx.ControlProxy {
        id: release

        group: root.group
        key: "release"
    }
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: root.lineColor
            strokeWidth: 2

            PathPolyline {
                path: root.points
            }
        }
    }
}
