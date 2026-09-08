import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import "../Theme"

Item {
    id: root

    required property var capabilities
    property alias drag: dragHandler
    readonly property var library: Mixxx.Library
    // The row's key as text, in the app's notation, read when the context
    // menu opens -- see contextMenu.onAboutToShow for why not earlier.
    property string menuKeyText: ""
    property alias tap: tapHandler

    // Asks the library to filter on this row's key, or on every key that
    // mixes with it. Relayed by SourceTree.qml up to Library.qml.
    signal keySearchRequested(string keyText, bool compatible)

    function hasCapabilities(caps) {
        return (root.capabilities & caps) == caps;
    }

    // The Track for this row, fetched when the user acts on it.
    //
    // The row delegate used to carry the model's Track role, and every menu
    // item here read it as `track`. Fetching that role hydrates the track --
    // a database read plus opening the audio file to parse its tags -- and
    // TableView fetched it for every cell it laid out, which froze the UI for
    // seconds on a resize. The delegate stopped requesting it, so `track` is
    // null and every one of these menu items silently did nothing: Load to
    // Deck, Add to Crate and Analyze.
    //
    // Ask for it here instead. One hydration when a menu item is triggered,
    // none during layout.
    function rowTrack() {
        return tableView && tableView.model ? tableView.model.getTrack(row) : null;
    }

    // Instantiator does not promise that its objects arrive in model order, and
    // Menu.insertItem() clamps an index that is past the end -- so inserting at
    // the index the Instantiator reports can leave the items permanently
    // shuffled. The deck menu came out 1, 4, 2, 3 that way. Place each item
    // after the ones that sort before it, which also keeps instantiated items
    // above any static entries below them.
    function insertMenuItemInOrder(menu, item) {
        let pos = 0;
        while (pos < menu.count) {
            const at = menu.itemAt(pos);
            if (at === null || at.menuIndex === undefined || at.menuIndex >= item.menuIndex)
                break;
            pos++;
        }
        menu.insertItem(pos, item);
    }

    component LibraryMenuItem: MenuItem {
        id: libraryMenuItem

        implicitHeight: 30
        implicitWidth: 210

        background: Rectangle {
            color: libraryMenuItem.highlighted
                    ? Qt.rgba(0.004, 0.863, 0.988, 0.18)
                    : "transparent"
            radius: 3
        }
        contentItem: Text {
            color: libraryMenuItem.enabled ? Theme.deckTextColor : Theme.midGray
            elide: Text.ElideRight
            font.pixelSize: 12
            leftPadding: 8
            rightPadding: 20
            text: libraryMenuItem.text
            verticalAlignment: Text.AlignVCenter
        }
    }
    component LibraryMenuSeparator: MenuSeparator {
        contentItem: Rectangle {
            color: Theme.midGray
            implicitHeight: 1
            opacity: 0.5
        }
    }
    component LibraryMenu: Menu {
        delegate: LibraryMenuItem {
        }
        padding: 4

        background: Rectangle {
            border.color: Theme.midGray
            border.width: 1
            color: Theme.darkGray2
            implicitWidth: 210
            radius: 4
        }
    }

    DragHandler {
        id: dragHandler

        target: value
    }
    TapHandler {
        id: tapHandler

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onLongPressed: mouse => {
            contextMenu.popup();
        }
        onTapped: (eventPoint, button) => {
            if (button === Qt.RightButton) {
                contextMenu.popup();
            }
        }
    }
    LibraryMenu {
        id: contextMenu

        title: qsTr("File")

        // The key entries name the key, which means hydrating the track (see
        // rowTrack). Doing that in a binding would hydrate every row as it is
        // laid out -- the freeze rowTrack() exists to avoid -- so it happens
        // once here, when the menu is actually opening.
        onAboutToShow: {
            const track = root.rowTrack();
            root.menuKeyText = track ? track.keyText : "";
        }

        LibraryMenu {
            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToPreviewDeck);
            }
            title: qsTr("Load to")

            LibraryMenu {
                id: loadToDeckMenu

                enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck)
                title: qsTr("Deck")

                Instantiator {
                    model: 4

                    delegate: LibraryMenuItem {
                        required property int index

                        readonly property int menuIndex: index

                        text: qsTr("Deck %1").arg(index + 1)

                        onTriggered: Mixxx.PlayerManager.getPlayer(`[Channel${index + 1}]`).loadTrack(root.rowTrack())
                    }

                    onObjectAdded: (index, object) => root.insertMenuItemInOrder(loadToDeckMenu, object)
                    onObjectRemoved: (index, object) => loadToDeckMenu.removeItem(object)
                }
            }
            LibraryMenu {
                enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler)
                title: qsTr("Sampler")
            }

            // Instantiator {
            //     id: recentFilesInstantiator
            //     model: settings.recentFiles
            //     delegate: MenuItem {
            //         text: settings.displayableFilePath(modelData)
            //         onTriggered: loadFile(modelData)
            //     }

            //     onObjectAdded: (index, object) => recentFilesMenu.insertItem(index, object)
            //     onObjectRemoved: (index, object) => recentFilesMenu.removeItem(object)
            // }
        }
        LibraryMenu {
            id: addToPlaylistMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet);
            }
            title: qsTr("Add to playlists")

            LibraryMenuSeparator {
            }
            LibraryMenuItem {
                enabled: false // TODO implement
                text: qsTr("Create New Playlist")
            }
        }
        LibraryMenu {
            id: addToCrateMenu

            // Refreshed each time the submenu opens so new crates show up.
            property var crates: []

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet);
            }
            title: qsTr("Crates")

            onAboutToShow: crates = library.crates()

            Instantiator {
                model: addToCrateMenu.crates

                delegate: LibraryMenuItem {
                    required property int index
                    required property var modelData

                    readonly property int menuIndex: index

                    enabled: !modelData.locked
                    text: modelData.name

                    onTriggered: library.addTrackToCrate(root.rowTrack(), modelData.id)
                }

                onObjectAdded: (index, object) => root.insertMenuItemInOrder(addToCrateMenu, object)
                onObjectRemoved: (index, object) => addToCrateMenu.removeItem(object)
            }
            LibraryMenuSeparator {
            }
            LibraryMenuItem {
                enabled: false // TODO implement
                text: qsTr("Create New Crate")
            }
        }
        LibraryMenu {
            id: analyzeMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.EditMetadata) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Analyze);
            }
            title: qsTr("Analyze")

            LibraryMenuItem {
                text: qsTr("Analyze")

                onTriggered: {
                    library.analyze(root.rowTrack());
                }
            }
            LibraryMenuItem {
                text: qsTr("Analyze all in view")

                onTriggered: {
                    tableView.model.analyzeAll();
                }
            }
            LibraryMenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze")
            }
            LibraryMenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze (constant BPM)")
            }
            LibraryMenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze (variable BPM)")
            }
        }
        LibraryMenuSeparator {
        }
        // Same searches the Camelot wheel runs on a click; disabled, with the
        // generic wording, when the row has no key to search on.
        LibraryMenuItem {
            enabled: root.menuKeyText.length > 0
            text: root.menuKeyText.length > 0 ? qsTr("Find tracks in %1").arg(root.menuKeyText) : qsTr("Find tracks in this key")

            onTriggered: root.keySearchRequested(root.menuKeyText, false)
        }
        LibraryMenuItem {
            enabled: root.menuKeyText.length > 0
            text: root.menuKeyText.length > 0 ? qsTr("Find keys compatible with %1").arg(root.menuKeyText) : qsTr("Find compatible keys")

            onTriggered: root.keySearchRequested(root.menuKeyText, true)
        }
    }
}
