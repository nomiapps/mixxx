import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Item {
    id: root

    property alias focusWidget: focusedWidgetControl.value

    signal moveVertical(int offset)
    signal scrollVertical(int offset)
    signal loadSelectedTrack(string group, bool play)
    signal loadSelectedTrackIntoNextAvailableDeck(bool play)

    Skin.FocusedWidgetControl {
        id: focusedWidgetControl

        Component.onCompleted: this.value = Skin.FocusedWidgetControl.WidgetKind.LibraryView
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "GoToItem"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "LoadSelectedIntoFirstStopped"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectTrackKnob"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectPrevTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectNextTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveVertical"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(value);
        }
    }

    // [Library] ScrollVertical/ScrollUp/ScrollDown are the PGUP/PGDN half of a
    // browse encoder (shift + browse on most controllers). The New UI handled
    // only the Move* half, so shifted browsing did nothing at all here.
    Mixxx.ControlProxy {
        group: "[Library]"
        key: "ScrollVertical"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.scrollVertical(value);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "ScrollUp"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.scrollVertical(-1);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "ScrollDown"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.scrollVertical(1);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveUp"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(-1);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveDown"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(1);
        }
    }

    Mixxx.ControlProxy {
        id: numDecksControl

        group: "[App]"
        key: "num_decks"
    }

    Instantiator {
        model: {
            const groups = [];
            for (let i = 0; i < numDecksControl.value; ++i) {
                groups.push("[Channel" + (i + 1) + "]");
            }
            return groups;
        }

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property string modelData

            group: modelData
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }

    Mixxx.ControlProxy {
        id: numPreviewDecksControl

        group: "[App]"
        key: "num_preview_decks"
    }

    Instantiator {
        model: {
            const groups = [];
            for (let i = 0; i < numPreviewDecksControl.value; ++i) {
                groups.push("[PreviewDeck" + (i + 1) + "]");
            }
            return groups;
        }

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property string modelData

            group: modelData
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }

    Mixxx.ControlProxy {
        id: numSamplersControl

        group: "[App]"
        key: "num_samplers"
    }

    Instantiator {
        // Model the groups themselves rather than a count. Raising the count
        // rebuilds the set -- main.qml raises num_samplers from the configured
        // value to 16 on init -- and Instantiator sets index to -1 on the
        // delegates it tears down, which re-evaluated a group binding built
        // from index to "[Sampler0]" and sent both control proxies looking for
        // a channel that does not exist. A group string survives the teardown
        // it is destroyed by.
        model: {
            const groups = [];
            for (let i = 0; i < numSamplersControl.value; ++i) {
                groups.push("[Sampler" + (i + 1) + "]");
            }
            return groups;
        }

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property string modelData

            group: modelData
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }
}
