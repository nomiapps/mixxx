pragma ComponentBehavior: Bound
import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// The whole New UI library browser as a surface element: source tree, search,
// smart crates and track list. Self-contained -- it builds its own sidebar from
// the Mixxx.Library singleton -- so it takes no group and ignores @left/@right.
//
// "loadTargets" adds a column of load buttons down the right-hand edge, one per
// entry: {"group": "[Channel2]", "label": "DECK 2"}. Without it the element is
// exactly what it always was, and loading goes through the library's own
// double-click -- which picks the first STOPPED deck, so on a surface with four
// decks you cannot say which one you meant. Each button loads THIS list's
// selection into the group it names.
Item {
    id: root

    // Entries without a group are dropped rather than carried: a ControlProxy
    // built on an empty group logs a warning on every lookup.
    readonly property var loadTargets: (root.spec.loadTargets ?? []).filter(t => ((t.group ?? "") !== ""))
    required property var spec
    property var surface: null
    readonly property real targetColumnWidth: root.loadTargets.length > 0 ? (root.spec.loadTargetWidth ?? 150) * root.uiScale : 0
    // Element-local pixels per canvas unit, so a width in the layout JSON means
    // the same thing here as it does in a rect. The rect in force is the one
    // "rectIf" may have swapped in, not necessarily spec.rect.
    readonly property real uiScale: {
        const box = root.surface ? root.surface.elementRect(root.spec) : root.spec.rect;
        return (box && box[2] > 0) ? root.width / box[2] : 1;
    }

    Skin.Library {
        id: library

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: targets.left
        anchors.top: parent.top
        clip: true
    }
    Column {
        id: targets

        readonly property real cellSpacing: 4 * root.uiScale

        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: cellSpacing
        width: root.targetColumnWidth

        Repeater {
            model: root.loadTargets

            Skin.Button {
                id: targetButton

                // A deck past [App],num_decks has no player at all, so the
                // button is dead rather than merely unwise. Show that.
                readonly property int deckNumber: {
                    const m = targetButton.group.match(/^\[Channel(\d+)\]$/);
                    return m ? Number(m[1]) : 0;
                }
                readonly property string group: targetButton.modelData.group ?? ""
                readonly property bool guardPlaying: targetButton.modelData.allowWhilePlaying !== true
                required property var modelData
                readonly property bool present: targetButton.deckNumber === 0 || targetButton.deckNumber <= numDecks.value

                activeColor: targetButton.modelData.color ?? Theme.deckActiveColor
                // Dim a target that is playing. Mixxx's own protection for this
                // ([Controls],AllowTrackLoadToPlayingDeck) lives in the widget
                // and drag-and-drop layer, NOT in the player, so the QML load
                // path used here would happily drop a new track onto a deck
                // mid-play. The surface has to hold that line itself, and the
                // lit buttons then read as "these decks are free". Preview and
                // sampler targets opt out with "allowWhilePlaying": true.
                // The preference itself is not reachable from QML today, so
                // this is unconditional rather than honouring it.
                enabled: targetButton.present && !(playControl.playing && targetButton.guardPlaying)
                height: (targets.height - targets.cellSpacing * (root.loadTargets.length - 1)) / Math.max(1, root.loadTargets.length)
                opacity: enabled ? 1 : 0.4
                text: targetButton.modelData.label ?? targetButton.group
                width: targets.width

                onClicked: library.trackList.loadSelectedTrack(targetButton.group, targetButton.modelData.play === true)

                Mixxx.ControlProxy {
                    id: numDecks

                    group: "[App]"
                    key: "num_decks"
                }
                Mixxx.ControlProxy {
                    id: playControl

                    readonly property bool playing: playControl.value > 0

                    group: targetButton.group
                    key: "play"
                }
            }
        }
    }
    // Inset frame, matching the other surface elements.
    Rectangle {
        anchors.fill: parent
        border.color: "#000000"
        border.width: 1
        color: "transparent"
        opacity: 0.5
    }
}
