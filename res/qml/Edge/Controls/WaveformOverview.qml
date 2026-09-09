import Mixxx 1.0 as Mixxx
import Edge.Controls 1.0 as EdgeControls
import QtQuick 2.12

Mixxx.WaveformOverview {
    id: root

    property color cueMarkerColor: "red"
    required property string group
    property color introOutroMarkerColor: "blue"
    property color loopMarkerColor: "green"
    property string playPositionMarkerColor: "white"
    readonly property var player: Mixxx.PlayerManager.getPlayer(root.group)

    track: player?.currentTrack

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"

        onValueChanged: value => {
            markers.visible = value;
        }
    }
    Mixxx.ControlProxy {
        id: playPositionControl

        group: root.group
        key: "playposition"
    }
    Item {
        id: markers

        anchors.fill: parent
        visible: trackLoadedControl.value

        EdgeControls.WaveformOverviewMarkerLayer {
            anchors.fill: parent
            cueColor: root.cueMarkerColor
            cueText: "C"
            group: root.group
            introOutroColor: root.introOutroMarkerColor
            introStartText: "IN"
            labelColor: "white"
            loopColor: root.loopMarkerColor
            loopStartText: "LOOP"
            outroStartText: "OUT"
            rangeEnd: root.rangeEnd
            rangeStart: root.rangeStart
            showHotcueLabels: false
            showIntroOutroLabels: false
            showLoopLabel: false
        }
        EdgeControls.WaveformOverviewMarker {
            id: playPositionMarker

            anchors.fill: parent
            color: root.playPositionMarkerColor
            group: root.group
            key: "playposition"
            rangeEnd: root.rangeEnd
            rangeStart: root.rangeStart
        }
    }
    MouseArea {
        id: seekArea

        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true

        // Seeking has to invert the view range too, or a click lands wherever
        // that fraction falls in the WHOLE track instead of where it was aimed.
        // Reached through the id rather than `this`: these handlers are arrow
        // functions, which take their `this` from the enclosing scope.
        function seek(x) {
            const span = root.rangeEnd - root.rangeStart;
            const fraction = x / seekArea.width;
            playPositionControl.value = span > 0 ? root.rangeStart + fraction * span : fraction;
        }

        onPositionChanged: mouse => {
            if (seekArea.containsPress)
                seekArea.seek(mouse.x);
        }
        onPressed: mouse => {
            seekArea.seek(mouse.x);
        }
    }
}
