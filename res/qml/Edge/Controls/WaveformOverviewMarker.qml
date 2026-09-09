import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Shapes
import QtQuick.Window 2.12

Item {
    id: root

    property string color: "white"
    required property string group
    required property string key
    // The slice of the track the overview beside us is drawing, as fractions of
    // its length. The marker has to map through the same window or it points at
    // where the position would be on a full-track waveform.
    property real rangeStart: 0
    property real rangeEnd: 1

    function mapX(position) {
        const span = root.rangeEnd - root.rangeStart;
        if (span <= 0)
            return root.width * position;
        return root.width * (position - root.rangeStart) / span;
    }

    Shape {
        id: shape

        anchors.fill: parent
        antialiasing: true
        layer.samples: 2
        layer.smooth: true
        // Qt 6.6+ resolution-independent antialiasing; the older
        // geometry renderer stair-steps curves on some displays.
        preferredRendererType: Shape.CurveRenderer
        visible: control.value >= 0

        ShapePath {
            startX: marker.x
            startY: 0
            strokeColor: root.color
            strokeWidth: 1

            PathLine {
                id: marker

                property bool hovered: false

                x: 0
                y: root.height
            }
        }
    }
    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
    }

    // Moved once per frame of this window rather than on every control update:
    // a render request that lands mid-frame parks the GUI thread until the
    // window's render thread is free, most of a frame period on the 60 Hz
    // Edge panel. Driven by the window's own afterFrameEnd, not FrameAnimation,
    // which the global animation driver ticks at the fastest window's rate.
    Connections {
        function onAfterFrameEnd() {
            // Math.round saves tons of CPU by avoiding redrawing for fractional pixel positions.
            const x = Math.round(root.mapX(control.value) * Screen.devicePixelRatio) / Screen.devicePixelRatio;
            if (x !== marker.x)
                marker.x = x;
        }

        target: root.Window.window
    }
}
