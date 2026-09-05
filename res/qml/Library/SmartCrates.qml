pragma ComponentBehavior: Bound
import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "../Theme"

// Smart crates: named saved searches. Saving stores the current search text
// verbatim; activating one hands the query back to the library shell, which
// switches to the whole collection and applies it.
Rectangle {
    id: root

    // The library search field's current text.
    required property string searchText
    // The query currently scoping the library, so the active crate can show as active.
    property string activeQuery: ""

    signal saved(string query)
    signal cleared

    signal activated(string query)

    color: Theme.backgroundColor

    ColumnLayout {
        anchors.bottomMargin: 7
        anchors.fill: parent
        anchors.leftMargin: 7
        anchors.rightMargin: 15
        anchors.topMargin: 7
        spacing: 5

        Rectangle {
            Layout.fillWidth: true
            color: Theme.toolbarBackgroundColor
            implicitHeight: 28
            radius: 3

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                spacing: 5

                Label {
                    Layout.fillWidth: true
                    color: Theme.deckTextColor
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    text: qsTr("SMART CRATES")
                }
                // Clicking the active crate again also clears it, but nothing said so.
                // An explicit way out beats a hidden toggle.
                Skin.FormButton {
                    implicitHeight: 22
                    text: qsTr("Show all")
                    visible: root.activeQuery.length > 0

                    onClicked: root.cleared()
                }
                Skin.FormButton {
                    enabled: root.searchText.trim().length > 0
                    implicitHeight: 22
                    text: qsTr("+ Save search")

                    onClicked: {
                        const query = root.searchText.trim();
                        Mixxx.Library.addSmartCrate(query, query);
                        // Hand it up so the search box can be cleared and the new crate
                        // become the scope: otherwise you are left both searching and
                        // scoped by the same text, which reads as a filtered library.
                        root.saved(query);
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: Theme.blue
                height: 1
                opacity: 0.35
            }
        }
        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            color: Theme.sunkenBackgroundColor

            Label {
                anchors.centerIn: parent
                color: Theme.midGray
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Save a search to create a smart crate")
                visible: smartCrateView.count == 0
                width: parent.width - 10
                wrapMode: Text.WordWrap
            }
            ListView {
                id: smartCrateView

                anchors.fill: parent
                clip: true
                model: Mixxx.Library.smartCrates

                ScrollBar.vertical: ScrollBar {
                }
                delegate: Rectangle {
                    id: row

                    required property int index
                    required property var modelData

                    readonly property bool active: root.activeQuery.length > 0 && root.activeQuery === row.modelData.query
                    property bool renaming: false

                    color: row.active ? Qt.rgba(0.004, 0.863, 0.988, 0.16) : (rowMouseArea.containsMouse ? Theme.knobBackgroundColor : 'transparent')
                    implicitHeight: 24
                    radius: 3
                    width: smartCrateView.width

                    MouseArea {
                        id: rowMouseArea

                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: root.activated(row.modelData.query)
                        // A crate is named after its own query when saved, which is fine
                        // for "stem" and unreadable for a long one. Double-click to rename.
                        onDoubleClicked: {
                            nameField.text = row.modelData.name;
                            row.renaming = true;
                            nameField.forceActiveFocus();
                            nameField.selectAll();
                        }
                    }
                    TextInput {
                        id: nameField

                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: removeButton.left
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        clip: true
                        color: Theme.white
                        font.pixelSize: 12
                        selectByMouse: true
                        visible: row.renaming

                        onAccepted: {
                            Mixxx.Library.renameSmartCrate(row.index, nameField.text);
                            row.renaming = false;
                        }
                        // Losing focus commits nothing: an abandoned edit should not
                        // silently rename the crate.
                        onActiveFocusChanged: {
                            if (!nameField.activeFocus) {
                                row.renaming = false;
                            }
                        }
                        Keys.onEscapePressed: row.renaming = false
                    }
                    Label {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: removeButton.left
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        color: row.active ? Theme.white : Theme.textColor
                        elide: Text.ElideRight
                        font.bold: row.active
                        font.pixelSize: 12
                        text: "⚙  " + row.modelData.name
                        visible: !row.renaming
                    }
                    Label {
                        id: removeButton

                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.textColor
                        font.pixelSize: 14
                        text: "✕"
                        visible: rowMouseArea.containsMouse || removeMouseArea.containsMouse

                        MouseArea {
                            id: removeMouseArea

                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true

                            onClicked: Mixxx.Library.removeSmartCrate(row.index)
                        }
                    }
                }
            }
        }
    }
}
