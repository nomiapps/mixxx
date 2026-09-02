import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import Qt5Compat.GraphicalEffects
import "Theme"

// A touch vinyl platter: cover art disc rotating at 33 1/3 rpm with the track,
// drag (finger or mouse) to scratch via scratch2. Built for the Edge surface
// window but size-agnostic.
Item {
    id: root

    required property string group
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)

    readonly property real positionSeconds: {
        const s = samplesControl.value / 2 / sampleRateControl.value * playPositionControl.value;
        return isNaN(s) ? 0 : s;
    }

    Mixxx.ControlProxy {
        id: playPositionControl

        group: root.group
        key: "playposition"
    }

    Mixxx.ControlProxy {
        id: samplesControl

        group: root.group
        key: "track_samples"
    }

    Mixxx.ControlProxy {
        id: sampleRateControl

        group: root.group
        key: "track_samplerate"
    }

    Mixxx.ControlProxy {
        id: scratchEnableControl

        group: root.group
        key: "scratch2_enable"
    }

    Mixxx.ControlProxy {
        id: scratchControl

        group: root.group
        key: "scratch2"
    }

    Rectangle {
        id: disc

        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height)
        height: width
        radius: width / 2
        color: "#0d0d0d"
        border.color: "#2a2a2a"
        border.width: 2
        // 33 1/3 rpm = 200 degrees per second
        rotation: (root.positionSeconds * 200) % 360

        // grooves
        Repeater {
            model: 4

            Rectangle {
                required property int index

                anchors.centerIn: parent
                width: disc.width * (0.92 - index * 0.07)
                height: width
                radius: width / 2
                color: "transparent"
                border.color: "#1f1f1f"
                border.width: 1
            }
        }

        Image {
            id: cover

            anchors.centerIn: parent
            width: parent.width * 0.5
            height: width
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
            source: root.deckPlayer ? root.deckPlayer.coverArtUrl : ""
            visible: false
        }

        Rectangle {
            id: coverMask

            anchors.fill: cover
            radius: width / 2
            visible: false
        }

        OpacityMask {
            anchors.fill: cover
            source: cover
            maskSource: coverMask
        }

        // position indicator stripe
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.02
            width: Math.max(4, disc.width * 0.012)
            height: parent.height * 0.2
            radius: width / 2
            color: Theme.white
        }
    }

    // Controller-style jog display, always in the hub like the hardware
    // (semi-translucent over cover art). Deliberately OUTSIDE the rotating
    // disc so the digits hold still.
    Item {
        id: jogDisplay

        anchors.centerIn: disc
        width: disc.width * 0.34
        height: width

        Mixxx.ControlProxy {
            id: bpmControl

            group: root.group
            key: "bpm"
        }

        Mixxx.ControlProxy {
            id: rateRatioControl

            group: root.group
            key: "rate_ratio"
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#e0101418"
            border.color: "#2e3a44"
            border.width: 2
        }

        Column {
            anchors.centerIn: parent
            spacing: jogDisplay.height * 0.02

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: bpmControl.value > 0 ? bpmControl.value.toFixed(1) : "--.-"
                color: Theme.white
                font.pixelSize: Math.max(14, jogDisplay.height * 0.22)
                font.bold: true
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "BPM"
                color: "#5a6a76"
                font.pixelSize: Math.max(8, jogDisplay.height * 0.07)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    const s = root.positionSeconds;
                    const m = Math.floor(s / 60);
                    const sec = Math.floor(s - m * 60);
                    return (m < 10 ? "0" : "") + m + ":" + (sec < 10 ? "0" : "") + sec;
                }
                color: Theme.deckActiveColor
                font.pixelSize: Math.max(11, jogDisplay.height * 0.14)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    const pct = (rateRatioControl.value - 1) * 100;
                    return (pct >= 0 ? "+" : "") + pct.toFixed(1) + "%";
                }
                color: Theme.bpmSliderBarColor
                font.pixelSize: Math.max(10, jogDisplay.height * 0.1)
            }
        }
    }

    MultiPointTouchArea {
        anchors.fill: disc
        maximumTouchPoints: 1
        mouseEnabled: true

        property real lastAngle: 0
        property double lastTime: 0

        function angleAt(x, y) {
            return Math.atan2(y - height / 2, x - width / 2) * 180 / Math.PI;
        }

        onPressed: (touchPoints) => {
            const p = touchPoints[0];
            lastAngle = angleAt(p.x, p.y);
            lastTime = Date.now();
            scratchControl.value = 0;
            scratchEnableControl.value = 1;
        }
        onUpdated: (touchPoints) => {
            const p = touchPoints[0];
            const now = Date.now();
            let delta = angleAt(p.x, p.y) - lastAngle;
            while (delta > 180) delta -= 360;
            while (delta < -180) delta += 360;
            const dt = Math.max(now - lastTime, 1) / 1000;
            // rate 1.0 = 200 deg/s forward
            scratchControl.value = (delta / 200) / dt;
            lastAngle += delta;
            lastTime = now;
        }
        onReleased: {
            scratchEnableControl.value = 0;
            scratchControl.value = 0;
        }
    }
}
