import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// Text readout for a deck. `spec.field` selects what to show (defaults to the
// original "artist - title" so existing layouts are unchanged):
//   title     artist - title
//   bpm       current (rate-adjusted) BPM
//   key       musical key
//   remaining time left in the track, as -m:ss
//   elapsed   time played, as m:ss
//   deckinfo  "BPM · KEY · -remaining" on one line
Item {
    id: root

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    readonly property string field: spec.field ?? "title"

    property var player: root.groupResolved ? Mixxx.PlayerManager.getPlayer(root.groupResolved) : null

    Mixxx.ControlProxy {
        id: bpmControl

        group: root.groupResolved
        key: "bpm"
    }

    Mixxx.ControlProxy {
        id: playPositionControl

        group: root.groupResolved
        key: "playposition"
    }

    Mixxx.ControlProxy {
        id: sampleRateControl

        group: root.groupResolved
        key: "track_samplerate"
    }

    Mixxx.ControlProxy {
        id: samplesControl

        group: root.groupResolved
        key: "track_samples"
    }

    readonly property bool loaded: root.player && root.player.isLoaded
    readonly property real durationSeconds: samplesControl.value / 2 / sampleRateControl.value
    readonly property real elapsedSeconds: durationSeconds * playPositionControl.value

    function formatTime(seconds) {
        if (!root.loaded || isNaN(seconds) || !isFinite(seconds))
            return "--:--";
        seconds = Math.max(0, seconds);
        const minutes = Math.floor(seconds / 60);
        let secs = Math.floor(seconds - minutes * 60);
        if (secs < 10)
            secs = "0" + secs;
        return minutes + ":" + secs;
    }

    readonly property string bpmText: (root.loaded && bpmControl.value > 0) ? bpmControl.value.toFixed(1) : "--"
    readonly property string keyText: (root.loaded && root.player.currentTrack?.keyText) ? root.player.currentTrack.keyText : "--"
    readonly property string remainingText: "-" + formatTime(durationSeconds - elapsedSeconds)
    readonly property string elapsedText: formatTime(elapsedSeconds)
    readonly property string titleText: root.player?.currentTrack ? (root.player.currentTrack.artist + " - " + root.player.currentTrack.title) : ""
    readonly property string deckInfoText: root.bpmText + " BPM  ·  " + root.keyText + "  ·  " + root.remainingText

    readonly property string displayText: {
        switch (root.field) {
        case "bpm":
            return root.bpmText;
        case "key":
            return root.keyText;
        case "remaining":
            return root.remainingText;
        case "elapsed":
            return root.elapsedText;
        case "deckinfo":
            return root.deckInfoText;
        case "titleinfo":
            return root.titleText + "   ·   " + root.deckInfoText;
        default:
            return root.titleText;
        }
    }

    Text {
        anchors.fill: parent
        text: root.displayText
        color: Theme.deckTextColor
        font.pixelSize: root.spec.fontSize ?? Math.max(10, root.height * 0.7)
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
