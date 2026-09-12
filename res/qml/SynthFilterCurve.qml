import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Shapes
import "Theme"

// A synth's low-pass filter as its response: gain against frequency on a
// log axis from 20 Hz to 20 kHz, +12 dB at the top of the box and -48 dB
// at the bottom, from the engine's own cutoff and resonance mapping. The
// resonance peak rises at the cutoff as the knob turns.
Rectangle {
    id: root

    readonly property int count: 48
    required property string group
    property color lineColor: Theme.blue
    readonly property var points: root.buildPoints(root.width, root.height, cutoff.value, resonance.value)

    function buildPoints(w, h, c, q) {
        const inset = 4;
        const span = w - 2 * inset;
        const topDb = 12;
        const bottomDb = -48;
        const pts = [];
        for (let i = 0; i < root.count; ++i) {
            const t = i / (root.count - 1);
            const hz = 20 * Math.pow(1000, t);
            const db = Math.max(bottomDb, Math.min(topDb, Mixxx.Synth.filterResponseDb(c, q, hz)));
            const y = inset + (h - 2 * inset) * (topDb - db) / (topDb - bottomDb);
            pts.push(Qt.point(inset + span * t, y));
        }
        return pts;
    }

    border.color: Theme.panelBorderColor
    color: Theme.sunkenBackgroundColor
    radius: 3

    Mixxx.ControlProxy {
        id: cutoff

        group: root.group
        key: "cutoff"
    }
    Mixxx.ControlProxy {
        id: resonance

        group: root.group
        key: "resonance"
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
