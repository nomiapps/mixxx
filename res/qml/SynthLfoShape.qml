import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Shapes
import "Theme"

// One cycle of a synth's LFO, drawn from the engine's own shape function so
// what is shown is what modulates, with a cursor at the phase the engine
// publishes (lfo_phase). Sample and hold shows four cycles, since one is a
// flat line.
Rectangle {
    id: root

    required property string group
    property color lineColor: Theme.purple
    readonly property var points: root.buildPoints(root.width, root.height, root.shape)
    property int shape: 0

    function buildPoints(w, h, shape) {
        const inset = 4;
        const span = w - 2 * inset;
        const mid = h / 2;
        const amp = (h - 2 * inset) / 2;
        const cycles = shape === 4 ? 4 : 1;
        const pts = [];
        for (let i = 0; i <= 64; ++i) {
            const phase = i / 64 * cycles;
            const value = Mixxx.Synth.lfoValue(shape, phase);
            pts.push(Qt.point(inset + span * i / 64, mid - value * amp));
        }
        return pts;
    }

    border.color: Theme.panelBorderColor
    color: Theme.sunkenBackgroundColor
    radius: 3

    Mixxx.ControlProxy {
        id: phaseControl

        group: root.group
        key: "lfo_phase"
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
    Rectangle {
        color: Theme.deckTextColor
        height: parent.height - 8
        opacity: 0.7
        width: 2
        x: 4 + (parent.width - 8) * phaseControl.value
        y: 4
    }
}
