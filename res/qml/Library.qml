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

    function applySearch() {
        if (root.sidebar && root.sidebar.tracklist) {
            root.sidebar.tracklist.search(searchField.text);
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
    // Smart crates are searches over the whole collection: show the first
    // source (all tracks) and then apply the saved query.
    function applySavedSearch(query) {
        if (root.sidebar) {
            root.sidebar.activate(root.sidebar.index(0, 0));
        }
        if (searchField.text === query) {
            root.applySearch();
        } else {
            searchField.text = query;
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
                searchText: searchField.text

                onActivated: query => root.applySavedSearch(query)
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
