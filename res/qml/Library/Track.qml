import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import "../Theme"

Item {
    id: root

    required property var capabilities
    property alias drag: dragHandler
    readonly property var library: Mixxx.Library
    property alias tap: tapHandler

    function hasCapabilities(caps) {
        return (root.capabilities & caps) == caps;
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
                        text: qsTr("Deck %1").arg(modelData + 1)

                        onTriggered: Mixxx.PlayerManager.getPlayer(`[Channel${modelData + 1}]`).loadTrack(track)
                    }

                    onObjectAdded: (index, object) => loadToDeckMenu.insertItem(index, object)
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
                    required property var modelData

                    enabled: !modelData.locked
                    text: modelData.name

                    onTriggered: library.addTrackToCrate(track, modelData.id)
                }

                onObjectAdded: (index, object) => addToCrateMenu.insertItem(index, object)
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
                    library.analyze(track);
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
    }
}
