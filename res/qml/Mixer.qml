import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "Theme"

Item {
    id: root

    required property var groups
    property bool show4decks: false

    implicitHeight: content.height + crossfader.height + 4
    implicitWidth: 190

    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundColor
        radius: 8
    }
    Column {
        anchors.fill: parent
        spacing: 4

        Item {
            id: content

            height: (root.show4decks ? eqDeck1.height * 2 : eqDeck1.height) + 10
            width: root.implicitWidth

            Rectangle {
                anchors.fill: parent
                border.color: "#303034"
                border.width: 1
                color: "#121214"
                radius: 7
            }
            Item {
                anchors.fill: parent
                anchors.margins: 5

                states: [
                    State {
                        name: "2decks"
                        when: root.show4decks

                        AnchorChanges {
                            target: eqDeck1

                            anchors {
                                left: eqDeck3.right
                                top: parent.top
                            }
                        }
                        AnchorChanges {
                            target: mixerDeck1

                            anchors {
                                left: mixerDeck3.right
                                top: eqDeck1.bottom
                            }
                        }
                        AnchorChanges {
                            target: mixerDeck2

                            anchors {
                                right: mixerDeck4.left
                                top: eqDeck2.bottom
                            }
                        }
                        AnchorChanges {
                            target: eqDeck2

                            anchors {
                                right: eqDeck4.left
                                top: parent.top
                            }
                        }
                    }
                ]

                Skin.EqColumn {
                    id: eqDeck3

                    group: root.groups[2]
                    visible: root.show4decks
                    width: 42

                    anchors {
                        left: parent.left
                        rightMargin: 4
                        top: parent.top
                    }
                }
                Skin.EqColumn {
                    id: eqDeck1

                    group: root.groups[0]
                    width: 42

                    anchors {
                        left: parent.left
                        leftMargin: root.show4decks ? 4 : 0
                        rightMargin: 4
                        top: parent.top
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck3

                    group: root.groups[2]
                    height: eqDeck2.height
                    visible: root.show4decks
                    width: 42

                    anchors {
                        left: parent.left
                        rightMargin: 4
                        top: eqDeck3.bottom
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck1

                    group: root.groups[0]
                    height: eqDeck1.height
                    width: 42

                    anchors {
                        left: eqDeck1.right
                        leftMargin: 4
                        rightMargin: 2
                        top: parent.top
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck2

                    group: root.groups[1]
                    height: eqDeck2.height
                    width: 42

                    anchors {
                        leftMargin: 2
                        right: eqDeck2.left
                        rightMargin: 4
                        top: parent.top
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck4

                    group: root.groups[3]
                    height: eqDeck2.height
                    visible: root.show4decks
                    width: 42

                    anchors {
                        leftMargin: 4
                        right: parent.right
                        top: eqDeck4.bottom
                    }
                }
                Skin.EqColumn {
                    id: eqDeck2

                    group: root.groups[1]
                    width: 42

                    anchors {
                        right: parent.right
                        // leftMargin: 4
                        rightMargin: root.show4decks ? 4 : 0
                        top: parent.top
                    }
                }
                Skin.EqColumn {
                    id: eqDeck4

                    group: root.groups[3]
                    visible: root.show4decks
                    width: 42

                    anchors {
                        leftMargin: 4
                        right: parent.right
                        top: parent.top
                    }
                }
            }
        }
        Item {
            id: crossfader

            height: 52
            width: root.implicitWidth

            Rectangle {
                anchors.fill: parent
                border.color: "#3a3a40"
                border.width: 1
                color: "#171719"
                radius: 7
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.top: parent.top
                color: Theme.blue
                opacity: 0.65
                radius: 1
                width: 2
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.top: parent.top
                color: Theme.yellow
                opacity: 0.65
                radius: 1
                width: 2
            }
            Item {
                anchors.fill: parent
                anchors.margins: 4

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: Theme.lightGray3
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.pixelSize: 8
                    font.letterSpacing: 1
                    text: root.show4decks ? qsTr("Deck assign · Crossfader") : qsTr("Crossfader")
                }
                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    color: Theme.blue
                    font.bold: true
                    font.pixelSize: 9
                    horizontalAlignment: Text.AlignHCenter
                    text: "A"
                    visible: root.show4decks
                    width: leftDeckAssignment.width
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    color: Theme.yellow
                    font.bold: true
                    font.pixelSize: 9
                    horizontalAlignment: Text.AlignHCenter
                    text: "B"
                    visible: root.show4decks
                    width: rightDeckAssignment.width
                }
                GridLayout {
                    id: leftDeckAssignment

                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    columnSpacing: 0
                    rowSpacing: 0
                    visible: root.show4decks
                    width: visible ? 40 : 0

                    Repeater {
                        model: root.show4decks ? 4 : 0

                        Item {
                            required property int index

                            Layout.column: show4decks ? index % 2 : 0
                            Layout.row: root.show4decks ? parseInt(index / 2) : index
                            implicitHeight: 20
                            implicitWidth: 20

                            Rectangle {
                                anchors.fill: parent
                                border.color: "#303034"
                                border.width: 1
                                color: "#101012"
                                radius: 3
                            }
                            Skin.ControlButton {
                                id: deckButton

                                activeColor: Theme.blue
                                fontPixelSize: 9
                                group: `[Channel${index + 1}]`
                                key: "orientation_left"
                                text: `D${index + 1}`
                                toggleable: true

                                anchors {
                                    fill: parent
                                    margins: 1
                                }
                            }
                        }
                    }
                }
                Skin.ControlFader {
                    id: crossfaderSlider

                    bar.color: Theme.crossfaderBarColor
                    bar.start: 0.5
                    bg: Theme.imgCrossfaderBackground
                    fg: Theme.imgCrossfaderHandle
                    group: "[Master]"
                    height: 33
                    key: "crossfader"
                    orientation: Qt.Horizontal

                    anchors {
                        left: leftDeckAssignment.right
                        leftMargin: 4
                        right: rightDeckAssignment.left
                        rightMargin: 4
                        top: parent.top
                        topMargin: 11
                    }
                    handleImage {
                        height: 31
                    }
                }
                GridLayout {
                    id: rightDeckAssignment

                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    columnSpacing: 0
                    rowSpacing: 0
                    visible: root.show4decks
                    width: visible ? 40 : 0

                    Repeater {
                        model: root.show4decks ? 4 : 0

                        Item {
                            required property int index

                            Layout.column: show4decks ? index % 2 : 0
                            Layout.row: root.show4decks ? parseInt(index / 2) : index
                            implicitHeight: 20
                            implicitWidth: 20

                            Rectangle {
                                anchors.fill: parent
                                border.color: "#303034"
                                border.width: 1
                                color: "#101012"
                                radius: 3
                            }
                            Skin.ControlButton {
                                id: deckButton

                                activeColor: Theme.yellow
                                fontPixelSize: 9
                                group: `[Channel${index + 1}]`
                                key: "orientation_right"
                                text: `D${index + 1}`
                                toggleable: true

                                anchors {
                                    fill: parent
                                    margins: 1
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
