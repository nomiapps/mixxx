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
            anchors.bottom: newCrateButton.top
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

                // Right-click / long-press a crate to rename or delete it.
                // Rename/delete are no-ops server-side if the entry is not a
                // real crate, so it is safe to offer on any sidebar row.
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: crateMenu.popup()
                    onPressAndHold: crateMenu.popup()
                }

                Menu {
                    id: crateMenu

                    MenuItem {
                        text: "Rename \"" + sidebarDelegate.display + "\""
                        onTriggered: crateNameDialog.openForRename(sidebarDelegate.display)
                    }
                    MenuItem {
                        text: "Delete \"" + sidebarDelegate.display + "\""
                        onTriggered: Mixxx.Library.deleteCrate(sidebarDelegate.display)
                    }
                }
            }
        }

        Button {
            id: newCrateButton

            anchors.left: parent.left
            anchors.bottom: smartCratePane.top
            anchors.leftMargin: 10
            anchors.bottomMargin: 4
            width: 200
            height: 24
            text: "+ New Crate"
            font.pixelSize: 11
            onClicked: crateNameDialog.openForCreate()
        }

        Dialog {
            id: crateNameDialog

            property bool renaming: false
            property string originalName: ""

            function openForCreate() {
                renaming = false;
                originalName = "";
                nameInput.text = "";
                open();
            }
            function openForRename(name) {
                renaming = true;
                originalName = name;
                nameInput.text = name;
                open();
            }

            anchors.centerIn: parent
            width: 320
            modal: true
            title: renaming ? "Rename crate" : "New crate"
            standardButtons: Dialog.Ok | Dialog.Cancel

            onAccepted: {
                if (renaming)
                    Mixxx.Library.renameCrate(originalName, nameInput.text);
                else
                    Mixxx.Library.createCrate(nameInput.text);
            }

            TextField {
                id: nameInput

                width: parent.width
                placeholderText: "Crate name"
                onAccepted: crateNameDialog.accept()
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

        Row {
            id: columnHeader

            property string sortRole: ""
            property bool sortDescending: false

            function toggleSort(role) {
                if (sortRole === role)
                    sortDescending = !sortDescending;
                else {
                    sortRole = role;
                    sortDescending = false;
                }
                Mixxx.Library.model.sortByRole(role, sortDescending);
            }

            anchors.top: searchField.bottom
            anchors.left: sidebarTree.right
            anchors.right: parent.right
            anchors.topMargin: 4
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            height: 22

            component HeaderCell: Item {
                required property string role
                required property string title
                required property real widthFraction

                width: (columnHeader.width - 4) * widthFraction
                height: columnHeader.height

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 4
                    color: columnHeader.sortRole === role ? Theme.blue : Theme.deckTextColor
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                    text: title + (columnHeader.sortRole === role ? (columnHeader.sortDescending ? "  v" : "  ^") : "")
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: columnHeader.toggleSort(role)
                }
            }

            HeaderCell { role: "artist"; title: "ARTIST"; widthFraction: 0.30 }
            HeaderCell { role: "title"; title: "TITLE"; widthFraction: 0.34 }
            HeaderCell { role: "bpm"; title: "BPM"; widthFraction: 0.09 }
            HeaderCell { role: "key"; title: "KEY"; widthFraction: 0.07 }
            HeaderCell { role: "duration"; title: "TIME"; widthFraction: 0.08 }
            HeaderCell { role: "genre"; title: "GENRE"; widthFraction: 0.12 }
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

            anchors.top: columnHeader.bottom
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
                required property string bpm
                required property string key
                required property string duration
                required property string genre

                readonly property color rowColor: (listView.currentIndex == itemDlgt.index && listView.activeFocus) ? Theme.blue : Theme.deckTextColor

                function loadToGroup(group) {
                    const player = Mixxx.PlayerManager.getPlayer(group);
                    if (!player)
                        return ;

                    player.loadTrackFromLocationUrl(itemDlgt.fileUrl, false);
                }

                function loadToNextDeck() {
                    if (!itemDlgt.fileUrl)
                        return ;

                    Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(itemDlgt.fileUrl, false);
                }

                implicitWidth: listView.width
                implicitHeight: 26

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 4

                    Text {
                        width: parent.width * 0.30
                        anchors.verticalCenter: parent.verticalCenter
                        text: itemDlgt.artist
                        color: itemDlgt.rowColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width * 0.34
                        anchors.verticalCenter: parent.verticalCenter
                        text: itemDlgt.title
                        color: itemDlgt.rowColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width * 0.09
                        anchors.verticalCenter: parent.verticalCenter
                        text: itemDlgt.bpm
                        color: itemDlgt.rowColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width * 0.07
                        anchors.verticalCenter: parent.verticalCenter
                        text: itemDlgt.key
                        color: itemDlgt.rowColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width * 0.08
                        anchors.verticalCenter: parent.verticalCenter
                        text: itemDlgt.duration
                        color: itemDlgt.rowColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width * 0.12
                        anchors.verticalCenter: parent.verticalCenter
                        text: itemDlgt.genre
                        color: itemDlgt.rowColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
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
                    onPressAndHold: {
                        listView.forceActiveFocus();
                        listView.currentIndex = itemDlgt.index;
                        loadMenu.popup();
                    }
                }

                MouseArea {
                    id: contextArea

                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onPressed: {
                        listView.forceActiveFocus();
                        listView.currentIndex = itemDlgt.index;
                        loadMenu.popup();
                    }
                }

                Menu {
                    id: loadMenu

                    MenuItem {
                        text: "Load to Deck 1"
                        onTriggered: itemDlgt.loadToGroup("[Channel1]")
                    }
                    MenuItem {
                        text: "Load to Deck 2"
                        onTriggered: itemDlgt.loadToGroup("[Channel2]")
                    }
                    MenuItem {
                        text: "Load to Deck 3"
                        onTriggered: itemDlgt.loadToGroup("[Channel3]")
                    }
                    MenuItem {
                        text: "Load to Deck 4"
                        onTriggered: itemDlgt.loadToGroup("[Channel4]")
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Load to next deck"
                        onTriggered: itemDlgt.loadToNextDeck()
                    }
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
