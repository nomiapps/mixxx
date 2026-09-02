import Mixxx 1.0 as Mixxx
import QtQml.Models
import QtQuick
import QtQuick.Controls
import "Theme"

Item {
    Rectangle {
        color: Theme.deckBackgroundColor
        anchors.fill: parent

        TreeView {
            id: sidebarTree

            anchors.top: parent.top
            anchors.bottom: smartCratePane.top
            anchors.left: parent.left
            anchors.margins: 10
            width: 200
            clip: true
            model: Mixxx.Library.sidebarModel
            selectionModel: ItemSelectionModel {}

            delegate: TreeViewDelegate {
                id: sidebarDelegate

                implicitWidth: sidebarTree.width
                font.pixelSize: 13
                palette.text: Theme.deckTextColor
                palette.buttonText: Theme.deckTextColor

                onClicked: {
                    Mixxx.Library.activateSidebarIndex(
                            sidebarTree.index(row, column));
                }
            }
        }

        Rectangle {
            id: smartCratePane

            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 10
            width: 200
            height: 168
            color: "transparent"

            Row {
                id: smartHeader

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 6

                Text {
                    text: "SMART CRATES"
                    color: Theme.deckTextColor
                    font.pixelSize: 11
                    font.bold: true
                    width: parent.width - saveSmartButton.width - 6
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    height: saveSmartButton.height
                }

                Button {
                    id: saveSmartButton

                    text: "+ save search"
                    font.pixelSize: 10
                    padding: 3
                    enabled: searchField.text.trim().length > 0
                    onClicked: Mixxx.Library.addSmartCrate(searchField.text, searchField.text)
                }
            }

            ListView {
                id: smartList

                anchors.top: smartHeader.bottom
                anchors.topMargin: 4
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                clip: true
                spacing: 2
                model: Mixxx.Library.smartCrates

                delegate: Rectangle {
                    required property int index
                    required property var modelData

                    width: smartList.width
                    height: 22
                    radius: 3
                    color: smartMouse.containsMouse ? Theme.knobBackgroundColor : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.right: removeSmart.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 6
                        text: "⚙  " + modelData.name
                        color: Theme.deckTextColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: smartMouse

                        anchors.fill: parent
                        anchors.rightMargin: 20
                        hoverEnabled: true
                        onClicked: Mixxx.Library.activateSmartCrate(index)
                    }

                    Text {
                        id: removeSmart

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 6
                        text: "✕"
                        color: Theme.deckTextColor
                        font.pixelSize: 11
                        opacity: 0.6

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            onClicked: Mixxx.Library.removeSmartCrate(index)
                        }
                    }
                }
            }
        }

        TextField {
            id: searchField

            anchors.top: parent.top
            anchors.left: sidebarTree.right
            anchors.right: parent.right
            anchors.margins: 10
            height: 28
            placeholderText: "Search..."
            color: Theme.deckTextColor
            placeholderTextColor: Theme.deckTextColor
            font.pixelSize: 13
            // debounced: the library search runs a real SQL query
            onTextChanged: searchDebounce.restart()

            background: Rectangle {
                color: Theme.knobBackgroundColor
                radius: 4
                border.color: searchField.activeFocus ? Theme.deckActiveColor : "transparent"
                border.width: 1
            }

            Keys.onPressed: (event) => {
                switch (event.key) {
                case Qt.Key_Escape:
                    searchField.text = "";
                    listView.forceActiveFocus();
                    event.accepted = true;
                    break;
                case Qt.Key_Down:
                case Qt.Key_Enter:
                case Qt.Key_Return:
                    listView.forceActiveFocus();
                    event.accepted = true;
                    break;
                }
            }

            Timer {
                id: searchDebounce

                interval: 300
                onTriggered: Mixxx.Library.search(searchField.text)
            }
        }

        LibraryControl {
            id: libraryControl

            onMoveVertical: (offset) => {
                listView.moveSelectionVertical(offset);
            }
            onLoadSelectedTrack: (group, play) => {
                listView.loadSelectedTrack(group, play);
            }
            onLoadSelectedTrackIntoNextAvailableDeck: (play) => {
                listView.loadSelectedTrackIntoNextAvailableDeck(play);
            }
            onFocusWidgetChanged: {
                switch (focusWidget) {
                    case FocusedWidgetControl.WidgetKind.LibraryView:
                        listView.forceActiveFocus();
                        break;
                }
            }
        }

        ListView {
            id: listView

            function moveSelectionVertical(value) {
                if (value == 0)
                    return ;

                const rowCount = model.rowCount();
                if (rowCount == 0)
                    return ;

                currentIndex = Mixxx.MathUtils.positiveModulo(currentIndex + value, rowCount);
            }

            function loadSelectedTrackIntoNextAvailableDeck(play) {
                const url = model.get(currentIndex).fileUrl;
                if (!url)
                    return ;

                Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(url, play);
            }

            function loadSelectedTrack(group, play) {
                const url = model.get(currentIndex).fileUrl;
                if (!url)
                    return ;

                const player = Mixxx.PlayerManager.getPlayer(group);
                if (!player)
                    return ;

                player.loadTrackFromLocationUrl(url, play);
            }

            anchors.top: searchField.bottom
            anchors.bottom: parent.bottom
            anchors.left: sidebarTree.right
            anchors.right: parent.right
            anchors.margins: 10
            clip: true
            keyNavigationWraps: true
            highlightMoveDuration: 250
            highlightResizeDuration: 50
            model: Mixxx.Library.model
            Keys.onPressed: (event) => {
                switch (event.key) {
                    case Qt.Key_Enter:
                        case Qt.Key_Return:
                            listView.loadSelectedTrackIntoNextAvailableDeck(false);
                        break;
                }
            }

            delegate: Item {
                id: itemDlgt

                required property int index
                required property url fileUrl
                required property string artist
                required property string title

                implicitWidth: listView.width
                implicitHeight: 30

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: itemDlgt.artist + " - " + itemDlgt.title
                    color: (listView.currentIndex == itemDlgt.index && listView.activeFocus) ? Theme.blue : Theme.deckTextColor

                    Behavior on color {
                        ColorAnimation {
                            duration: listView.highlightMoveDuration
                        }
                    }
                }

                Image {
                    id: dragItem

                    Drag.active: dragArea.drag.active
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.CopyAction
                    Drag.mimeData: {
                        "text/uri-list": itemDlgt.fileUrl,
                        "text/plain": itemDlgt.fileUrl
                    }
                    anchors.fill: parent
                }

                MouseArea {
                    id: dragArea

                    anchors.fill: parent
                    drag.target: dragItem
                    onPressed: {
                        listView.forceActiveFocus();
                        listView.currentIndex = itemDlgt.index;
                        parent.grabToImage((result) => {
                                dragItem.Drag.imageSource = result.url;
                        });
                    }
                    onDoubleClicked: listView.loadSelectedTrackIntoNextAvailableDeck(false)
                }
            }

            highlight: Rectangle {
                border.color: listView.activeFocus ? Theme.blue : Theme.deckTextColor
                border.width: 1
                color: "transparent"
            }
        }
    }
}
