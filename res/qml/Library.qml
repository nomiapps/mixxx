import "." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import QtQuick.Shapes
import "Theme"
import "Library" as LibraryComponent

Item {
    id: root

    property var sidebar: librarySources.sidebar()

    // A smart crate is a saved query that acts as its own SCOPE, independent of the
    // search box. It used to be applied BY writing into the search field, which meant
    // opening a crate silently left the library filtered with no sign of it, and typing
    // a search threw the crate away.
    property string smartCrateQuery: ""

    function applySearch() {
        if (root.sidebar && root.sidebar.tracklist) {
            // In practice only one of these is ever set -- focusing the search box drops
            // the crate scope. They are still ANDed rather than one overriding the other,
            // so nothing silently wins if that ever changes.
            const parts = [root.smartCrateQuery, searchField.text].filter(p => p.trim().length > 0);
            root.sidebar.tracklist.search(parts.join(" "));
        }
    }
    function focusSearch() {
        searchDebounce.stop();
        root.applySearch();
        searchField.forceActiveFocus(Qt.ShortcutFocusReason);
        searchField.selectAll();
    }
    function analyzeCurrentView() {
        if (!root.sidebar || !root.sidebar.tracklist) {
            analyzeToast.show(0);
            return;
        }
        analyzeToast.show(root.sidebar.tracklist.analyzeAll());
    }
    // Smart crates are searches over the whole collection: show the first source (all
    // tracks), then scope to the saved query. Selecting the active crate again clears
    // the scope, so there is always a way back to the full library.
    function applySavedSearch(query) {
        if (root.sidebar) {
            root.sidebar.activate(root.sidebar.index(0, 0));
        }
        root.smartCrateQuery = (root.smartCrateQuery === query) ? "" : query;
        searchDebounce.stop();
        root.applySearch();
    }

    // What makes a smart crate smart: it stores a QUERY, not a track list, so anything
    // added later that matches belongs to it. That only shows while you are looking at
    // one if the query is re-run when the collection changes -- otherwise a crate quietly
    // shows a stale set until you reselect it.
    Connections {
        function onLibraryScanActiveChanged() {
            if (!Mixxx.Library.libraryScanActive) {
                refreshDebounce.restart();
            }
        }

        // A track edited into or out of the query belongs in or out of the crate at
        // once. Debounced because analysis emits this per track and re-running the
        // query on every one would thrash the model during a scan.
        function onLibraryTracksChanged() {
            refreshDebounce.restart();
        }

        target: Mixxx.Library
    }

    Timer {
        id: refreshDebounce

        interval: 400

        onTriggered: {
            searchDebounce.stop();
            root.applySearch();
        }
    }

    LibraryComponent.SourceTree {
        id: librarySources
    }
    TextField {
        id: searchField

        anchors.left: parent.left
        anchors.right: rescanButton.left
        anchors.rightMargin: 6
        anchors.top: parent.top
        anchors.topMargin: 4
        height: 28
        color: Theme.deckTextColor
        font.pixelSize: 13
        placeholderText: qsTranslate("WSearchLineEdit", "Search...")
        placeholderTextColor: Theme.midGray

        background: Rectangle {
            border.color: searchField.activeFocus ? Theme.blue : Theme.midGray
            border.width: 1
            color: Theme.knobBackgroundColor
            radius: 4
        }

        onTextChanged: searchDebounce.restart()
        // Reaching for the search box means searching the whole library, so drop any
        // crate scope rather than quietly searching inside it. Otherwise a search can
        // return nothing while the collection plainly contains matches.
        onActiveFocusChanged: {
            if (searchField.activeFocus && root.smartCrateQuery.length > 0) {
                root.smartCrateQuery = "";
                searchDebounce.stop();
                root.applySearch();
            }
        }

        Timer {
            id: searchDebounce

            interval: 300
            onTriggered: root.applySearch()
        }
    }
    Skin.FormButton {
        id: rescanButton

        anchors.right: analyzeViewButton.left
        anchors.rightMargin: 6
        anchors.top: parent.top
        anchors.topMargin: 4
        enabled: !Mixxx.Library.libraryScanActive
        height: 28
        text: Mixxx.Library.libraryScanActive ? qsTr("Scanning…") : qsTr("↻ Rescan")
        width: 90

        onClicked: Mixxx.Library.rescanLibrary()
    }
    Skin.FormButton {
        id: analyzeViewButton

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 4
        height: 28
        text: qsTr("⚡ Analyze view")
        width: 112

        onClicked: root.analyzeCurrentView()
    }
    Rectangle {
        id: analyzeToast

        function show(count) {
            toastText.text = count > 0
                    ? qsTr("Queued %n track(s) for analysis", "", count)
                    : qsTr("Nothing to analyze in this view");
            opacity = 1;
            toastTimer.restart();
        }

        anchors.right: parent.right
        anchors.top: analyzeViewButton.bottom
        anchors.topMargin: 6
        border.color: Theme.blue
        border.width: 1
        color: Theme.knobBackgroundColor
        height: 26
        opacity: 0
        radius: 4
        width: toastText.implicitWidth + 20
        z: 20

        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }
        Text {
            id: toastText

            anchors.centerIn: parent
            color: Theme.deckTextColor
            font.pixelSize: 11
        }
        Timer {
            id: toastTimer

            interval: 2400
            onTriggered: analyzeToast.opacity = 0
        }
    }
    SplitView {
        id: librarySplitView

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: searchField.bottom
        anchors.topMargin: 4
        orientation: Qt.Horizontal

        handle: Rectangle {
            id: handleDelegate

            property color handleColor: SplitHandle.pressed || SplitHandle.hovered ? Theme.panelSplitterHandleActive : Theme.panelSplitterHandle
            property int handleSize: SplitHandle.pressed || SplitHandle.hovered ? 6 : 5

            clip: true
            color: Theme.panelSplitterBackground
            implicitHeight: 8
            implicitWidth: 8

            containmentMask: Item {
                height: librarySplitView.height
                width: 8
                x: (handleDelegate.width - width) / 2
            }

            ColumnLayout {
                anchors.centerIn: parent

                Repeater {
                    model: 3

                    Rectangle {
                        color: handleColor
                        height: handleSize
                        radius: handleSize
                        width: handleSize
                    }
                }
            }
        }

        SplitView {
            id: sideBarSplitView

            SplitView.maximumWidth: 550
            SplitView.minimumWidth: 150
            SplitView.preferredWidth: root.width * 0.15
            orientation: Qt.Vertical

            handle: Rectangle {
                id: handleDelegate

                property color handleColor: SplitHandle.pressed || SplitHandle.hovered ? Theme.panelSplitterHandleActive : Theme.panelSplitterHandle
                property int handleSize: SplitHandle.pressed || SplitHandle.hovered ? 6 : 5

                clip: true
                color: Theme.panelSplitterBackground
                implicitHeight: 8
                implicitWidth: 8

                containmentMask: Item {
                    height: 8
                    width: sideBarSplitView.width
                    x: (handleDelegate.width - width) / 2
                }

                RowLayout {
                    anchors.centerIn: parent

                    Repeater {
                        model: 3

                        Rectangle {
                            color: handleColor
                            height: handleSize
                            radius: handleSize
                            width: handleSize
                        }
                    }
                }
            }

            LibraryComponent.Browser {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 200
                SplitView.preferredHeight: 500
                model: root.sidebar
            }
            LibraryComponent.SmartCrates {
                SplitView.minimumHeight: 60
                SplitView.preferredHeight: 130
                activeQuery: root.smartCrateQuery
                searchText: searchField.text

                onActivated: query => root.applySavedSearch(query)
                // Saving files the search away and returns to the full library. Leaving
                // the new crate selected looked broken: the box emptied but the library
                // stayed cut down, so it read as a search that had not cleared.
                onSaved: {
                    searchDebounce.stop();
                    searchField.text = "";
                    root.smartCrateQuery = "";
                    root.applySearch();
                }
                onCleared: {
                    searchDebounce.stop();
                    root.smartCrateQuery = "";
                    root.applySearch();
                }
            }
            Skin.PreviewDeck {
                SplitView.maximumHeight: 200
                SplitView.minimumHeight: 100
                SplitView.preferredHeight: 100
            }
        }
        LibraryComponent.TrackList {
            SplitView.fillHeight: true

            // FIXME: this is necessary to prevent the header label to render outside of the table when horizontally scrolling: https://github.com/mixxxdj/mixxx/pull/14514#issuecomment-3311914346
            clip: true
            model: root.sidebar.tracklist
        }
    }
}
