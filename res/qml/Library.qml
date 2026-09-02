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

            // Shared per-column pixel widths (header + rows bind to these);
            // draggable splitters below adjust them. GENRE takes the rest.
            property real colArtist: 300
            property real colTitle: 340
            property real colBpm: 90
            property real colKey: 70
            property real colTime: 80
            readonly property real colGenre: Math.max(60,
                    width - colArtist - colTitle - colBpm - colKey - colTime - 5 * 6)

            function toggleSort(role) {
                if (sortRole === role)
                    sortDescending = !sortDescending;
                else {
                    sortRole = role;
                    sortDescending = false;
                }
                Mixxx.Library.model.sortByRole(role, sortDescending);
                saveState();
            }

            function saveState() {
                Mixxx.Library.saveViewState("libraryColumns", {
                    "artist": colArtist, "title": colTitle, "bpm": colBpm,
                    "key": colKey, "time": colTime,
                    "sortRole": sortRole, "sortDescending": sortDescending
                });
            }

            Component.onCompleted: {
                const st = Mixxx.Library.loadViewState("libraryColumns");
                if (st) {
                    if (st.artist) colArtist = st.artist;
                    if (st.title) colTitle = st.title;
                    if (st.bpm) colBpm = st.bpm;
                    if (st.key) colKey = st.key;
                    if (st.time) colTime = st.time;
                    if (st.sortRole) {
                        sortRole = st.sortRole;
                        sortDescending = st.sortDescending === true;
                        Mixxx.Library.model.sortByRole(sortRole, sortDescending);
                    }
                }
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
                required property real cellWidth

                width: cellWidth
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

            // A draggable divider; emits horizontal delta in columnHeader coords.
            component Splitter: Item {
                signal dragged(real dx)

                width: 6
                height: columnHeader.height

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 1
                    height: parent.height
                    color: Theme.deckTextColor
                    opacity: splitMouse.pressed ? 0.8 : 0.25
                }

                MouseArea {
                    id: splitMouse

                    anchors.fill: parent
                    anchors.margins: -2
                    cursorShape: Qt.SplitHCursor
                    property real lastX: 0
                    onPressed: (mouse) => {
                        lastX = mapToItem(columnHeader, mouse.x, 0).x;
                    }
                    onPositionChanged: (mouse) => {
                        const cx = mapToItem(columnHeader, mouse.x, 0).x;
                        parent.dragged(cx - lastX);
                        lastX = cx;
                    }
                    onReleased: columnHeader.saveState()
                }
            }

            HeaderCell { role: "artist"; title: "ARTIST"; cellWidth: columnHeader.colArtist }
            Splitter { onDragged: (dx) => columnHeader.colArtist = Math.max(40, columnHeader.colArtist + dx) }
            HeaderCell { role: "title"; title: "TITLE"; cellWidth: columnHeader.colTitle }
            Splitter { onDragged: (dx) => columnHeader.colTitle = Math.max(40, columnHeader.colTitle + dx) }
            HeaderCell { role: "bpm"; title: "BPM"; cellWidth: columnHeader.colBpm }
            Splitter { onDragged: (dx) => columnHeader.colBpm = Math.max(30, columnHeader.colBpm + dx) }
            HeaderCell { role: "key"; title: "KEY"; cellWidth: columnHeader.colKey }
            Splitter { onDragged: (dx) => columnHeader.colKey = Math.max(30, columnHeader.colKey + dx) }
            HeaderCell { role: "duration"; title: "TIME"; cellWidth: columnHeader.colTime }
            Splitter { onDragged: (dx) => columnHeader.colTime = Math.max(30, columnHeader.colTime + dx) }
            HeaderCell { role: "genre"; title: "GENRE"; cellWidth: columnHeader.colGenre }
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

                    component Cell: Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: itemDlgt.rowColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    // Widths mirror the header columns; +6 absorbs each header
                    // splitter gap so cells stay aligned with their labels.
                    Cell { width: columnHeader.colArtist + 6; text: itemDlgt.artist }
                    Cell { width: columnHeader.colTitle + 6; text: itemDlgt.title }
                    Cell { width: columnHeader.colBpm + 6; text: itemDlgt.bpm }
                    Cell { width: columnHeader.colKey + 6; text: itemDlgt.key }
                    Cell { width: columnHeader.colTime + 6; text: itemDlgt.duration }
                    Cell { width: columnHeader.colGenre; text: itemDlgt.genre }
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
                    MenuSeparator {}
                    Menu {
                        id: addToCrateMenu

                        title: "Add to crate"

                        // Rebuild the crate list each time the menu opens.
                        onAboutToShow: {
                            crateRepeater.model = Mixxx.Library.crateNames();
                        }

                        Repeater {
                            id: crateRepeater

                            model: []
                            MenuItem {
                                required property string modelData
                                text: modelData
                                onTriggered: Mixxx.Library.addTrackUrlToCrate(
                                        itemDlgt.fileUrl, modelData)
                            }
                        }
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
