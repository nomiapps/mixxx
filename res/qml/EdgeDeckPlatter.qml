pragma ComponentBehavior: Bound

import Mixxx 1.0 as Mixxx
import QtQuick
import Qt5Compat.GraphicalEffects
import "Theme"

// A touch vinyl platter: cover art disc rotating at 33 1/3 rpm with the track,
// drag (finger or mouse) to scratch via scratch2. Built for the Edge surface
// window but size-agnostic.
Item {
    id: root

    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)

    // The playposition control updates at engine-visual rate (well below the
    // display refresh), which makes a directly-bound disc step visibly.
    // displaySeconds is a per-frame integration of the estimated velocity,
    // pulled toward the control's predicted position and snapped on seeks.
    property real displaySeconds: 0
    required property string group
    property real lastCoPos: 0
    property double lastCoTime: 0
    readonly property real positionSeconds: {
        const s = samplesControl.value / 2 / sampleRateControl.value * playPositionControl.value;
        return isNaN(s) ? 0 : s;
    }
    property real velocitySeconds: 0

    onPositionSecondsChanged: {
        const now = Date.now() / 1000;
        const pos = positionSeconds;
        if (lastCoTime > 0) {
            const dt = now - lastCoTime;
            if (dt > 0.001 && dt < 0.5)
                velocitySeconds = (pos - lastCoPos) / dt;
        }
        if (Math.abs(pos - displaySeconds) > 0.25)
            displaySeconds = pos; // seek/jump: snap, don't glide
        lastCoPos = pos;
        lastCoTime = now;
    }

    FrameAnimation {
        running: root.visible

        onTriggered: {
            const now = Date.now() / 1000;
            if (now - root.lastCoTime > 0.3) {
                // no updates: paused/stopped - stop the disc
                root.velocitySeconds = 0;
                return;
            }
            root.displaySeconds += root.velocitySeconds * frameTime;
            const predicted = root.lastCoPos + root.velocitySeconds * (now - root.lastCoTime);
            root.displaySeconds += (predicted - root.displaySeconds) * 0.15;
        }
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
        border.color: "#2a2a2a"
        border.width: 2
        color: "#0d0d0d"
        height: width
        radius: width / 2
        // 33 1/3 rpm = 200 degrees per second
        rotation: (root.displaySeconds * 200) % 360
        width: Math.min(parent.width, parent.height)

        // grooves
        Repeater {
            model: 4

            Rectangle {
                required property int index

                anchors.centerIn: parent
                border.color: "#1f1f1f"
                border.width: 1
                color: "transparent"
                height: width
                radius: width / 2
                width: disc.width * (0.92 - index * 0.07)
            }
        }
        Image {
            id: cover

            anchors.centerIn: parent
            height: width
            source: root.deckPlayer?.currentTrack?.coverArtUrl ?? ""
            sourceSize.height: height * Screen.devicePixelRatio
            sourceSize.width: width * Screen.devicePixelRatio
            visible: false
            width: parent.width * 0.5
        }
        Rectangle {
            id: coverMask

            anchors.fill: cover
            radius: width / 2
            visible: false
        }
        OpacityMask {
            anchors.fill: cover
            maskSource: coverMask
            source: cover
        }

        // position indicator stripe
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.white
            height: parent.height * 0.2
            radius: width / 2
            width: Math.max(4, disc.width * 0.012)
            y: parent.height * 0.02
        }
    }

    // Controller-style jog display, always in the hub like the hardware
    // (semi-translucent over cover art). Deliberately OUTSIDE the rotating
    // disc so the digits hold still.
    Item {
        id: jogDisplay

        anchors.centerIn: disc
        height: width
        width: disc.width * 0.34

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
            border.color: "#2e3a44"
            border.width: 2
            color: "#e0101418"
            radius: width / 2
        }
        Column {
            anchors.centerIn: parent
            spacing: jogDisplay.height * 0.02

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.white
                font.bold: true
                font.pixelSize: Math.max(14, jogDisplay.height * 0.22)
                text: bpmControl.value > 0 ? bpmControl.value.toFixed(1) : "--.-"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#5a6a76"
                font.pixelSize: Math.max(8, jogDisplay.height * 0.07)
                text: "BPM"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.deckActiveColor
                font.pixelSize: Math.max(11, jogDisplay.height * 0.14)
                text: {
                    const s = Math.max(0, root.positionSeconds);
                    const m = Math.floor(s / 60);
                    const sec = Math.floor(s - m * 60);
                    return (m < 10 ? "0" : "") + m + ":" + (sec < 10 ? "0" : "") + sec;
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.bpmSliderBarColor
                font.pixelSize: Math.max(10, jogDisplay.height * 0.1)
                text: {
                    const pct = (rateRatioControl.value - 1) * 100;
                    return (pct >= 0 ? "+" : "") + pct.toFixed(1) + "%";
                }
            }
        }
    }
    MultiPointTouchArea {
        property real lastAngle: 0
        property double lastTime: 0

        function angleAt(x, y) {
            return Math.atan2(y - height / 2, x - width / 2) * 180 / Math.PI;
        }

        anchors.fill: disc
        maximumTouchPoints: 1
        mouseEnabled: true

        onPressed: touchPoints => {
            const p = touchPoints[0];
            lastAngle = angleAt(p.x, p.y);
            lastTime = Date.now();
            scratchControl.value = 0;
            scratchEnableControl.value = 1;
        }
        onReleased: {
            scratchEnableControl.value = 0;
            scratchControl.value = 0;
        }
        onUpdated: touchPoints => {
            const p = touchPoints[0];
            const now = Date.now();
            let delta = angleAt(p.x, p.y) - lastAngle;
            while (delta > 180)
                delta -= 360;
            while (delta < -180)
                delta += 360;
            const dt = Math.max(now - lastTime, 1) / 1000;
            // rate 1.0 = 200 deg/s forward
            scratchControl.value = (delta / 200) / dt;
            lastAngle += delta;
            lastTime = now;
        }
    }
}
