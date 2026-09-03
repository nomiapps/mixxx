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

    signal activated(string query)

    color: Theme.backgroundColor

    ColumnLayout {
        anchors.bottomMargin: 7
        anchors.fill: parent
        anchors.leftMargin: 7
        anchors.rightMargin: 15
        anchors.topMargin: 7
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Label {
                Layout.fillWidth: true
                color: Theme.textColor
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: 14
                font.weight: Font.Bold
                text: qsTr("Smart crates")
            }
            Skin.FormButton {
                enabled: root.searchText.trim().length > 0
                text: qsTr("Save search")

                onClicked: {
                    const query = root.searchText.trim();
                    Mixxx.Library.addSmartCrate(query, query);
                }
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

                    color: rowMouseArea.containsMouse ? Theme.midGray : 'transparent'
                    implicitHeight: 30
                    width: smartCrateView.width

                    MouseArea {
                        id: rowMouseArea

                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: root.activated(row.modelData.query)
                    }
                    Label {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: removeButton.left
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.textColor
                        elide: Text.ElideRight
                        font.pixelSize: 14
                        text: row.modelData.name
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
