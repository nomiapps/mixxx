import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.12

Item {
    id: root

    required property string group
    property real rpm: 33
    property bool indicatorVisible: true
    property alias indicator: indicatorContainer.contentItem
    readonly property real positionSeconds: {
        const totalFrames = samplesControl.value / 2;
        return (!isNaN(sampleRateControl.value) && sampleRateControl.value > 0) ? playPositionControl.value * totalFrames / sampleRateControl.value : 0;
    }
    property real displaySeconds: 0
    property real velocitySeconds: 0
    property real lastPositionSeconds: 0
    property double lastPositionTime: 0

    onPositionSecondsChanged: {
        const now = Date.now() / 1000;
        if (lastPositionTime > 0) {
            const dt = now - lastPositionTime;
            if (dt > 0.001 && dt < 0.5)
                velocitySeconds = (positionSeconds - lastPositionSeconds) / dt;
            if (Math.abs(positionSeconds - displaySeconds) > 0.25)
                displaySeconds = positionSeconds;
        } else {
            displaySeconds = positionSeconds;
        }
        lastPositionSeconds = positionSeconds;
        lastPositionTime = now;
    }

    FrameAnimation {
        running: root.visible && root.indicatorVisible
        onTriggered: {
            const now = Date.now() / 1000;
            if (now - root.lastPositionTime > 0.3) {
                root.velocitySeconds = 0;
                return;
            }
            root.displaySeconds += root.velocitySeconds * frameTime;
            const predicted = root.lastPositionSeconds + root.velocitySeconds * (now - root.lastPositionTime);
            root.displaySeconds += (predicted - root.displaySeconds) * 0.15;
        }
    }

    // Avoid animation short blinking of spinny during startup
    Component.onCompleted: indicatorTransition.enabled = true

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
        id: playPositionControl

        group: root.group
        key: "playposition"
    }

    Control {
        id: indicatorContainer

        anchors.fill: parent
        visible: opacity > 0

        contentItem: Rectangle {
            height: root.height / 2
            width: height / 12
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
        }

        transform: Rotation {
            id: indicatorRotation

            property real roundsPerSecond: root.rpm / 60
            property real rotationFactor: indicatorRotation.roundsPerSecond * root.displaySeconds % 1

            origin.x: root.width / 2
            origin.y: root.height / 2
            angle: 360 * rotationFactor
        }

        states: State {
            name: "hidden"
            when: !root.indicatorVisible

            PropertyChanges {
                target: indicatorContainer
                opacity: 0
            }
        }

        transitions: Transition {
            id: indicatorTransition

            enabled: false
            to: "hidden"
            reversible: true

            PropertyAnimation {
                property: "opacity"
                duration: 150
            }
        }
    }
}
