import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Window 2.12
import "Theme"

// The Edge performance surface: a second window shaped for a 2560x720
// touchscreen. Two touch platters, hotcue pads, transport, crossfader.
// Every touch area is independent, so both platters scratch at once.
Window {
    id: root

    width: 2560
    height: 720
    color: Theme.backgroundColor
    title: "Mixxx - Edge Surface"

    component DeckSide: Item {
        id: side

        required property string group
        property var player: Mixxx.PlayerManager.getPlayer(group)

        Text {
            id: trackText

            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            text: side.player ? (side.player.artist + " - " + side.player.title) : ""
            color: Theme.deckTextColor
            font.pixelSize: 20
            elide: Text.ElideRight
            width: parent.width * 0.9
            horizontalAlignment: Text.AlignHCenter
        }

        Skin.EdgeDeckPlatter {
            id: platter

            group: side.group
            anchors.top: trackText.bottom
            anchors.bottom: padsGrid.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 4
        }

        Grid {
            id: padsGrid

            anchors.bottom: transportRow.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 8
            columns: 4
            spacing: 6

            Repeater {
                model: 8

                Skin.HotcueButton {
                    required property int index

                    width: 110
                    height: 54
                    hotcueNumber: index + 1
                    group: side.group
                }
            }
        }

        Row {
            id: transportRow

            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 6
            spacing: 16

            Skin.ControlButton {
                width: 150
                height: 44
                group: side.group
                key: "cue_default"
                text: "CUE"
                activeColor: Theme.deckActiveColor
            }

            Skin.ControlButton {
                width: 150
                height: 44
                group: side.group
                key: "play"
                text: "PLAY"
                toggleable: true
                activeColor: Theme.deckActiveColor
            }
        }
    }

    DeckSide {
        id: leftDeck

        group: "[Channel1]"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: center.left
        anchors.margins: 8
    }

    Item {
        id: center

        width: 280
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter

        Skin.ControlSlider {
            id: volume1

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 60
            height: parent.height * 0.6
            orientation: Qt.Vertical
            group: "[Channel1]"
            key: "volume"
            barColor: Theme.volumeSliderBarColor
            bg: Theme.imgVolumeSliderBackground
        }

        Skin.ControlSlider {
            id: volume2

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 60
            height: parent.height * 0.6
            orientation: Qt.Vertical
            group: "[Channel2]"
            key: "volume"
            barColor: Theme.volumeSliderBarColor
            bg: Theme.imgVolumeSliderBackground
        }

        Skin.ControlSlider {
            id: crossfader

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: 44
            orientation: Qt.Horizontal
            group: "[Master]"
            key: "crossfader"
            barColor: Theme.crossfaderBarColor
            barStart: 0.5
            fg: Theme.imgCrossfaderHandle
            bg: Theme.imgCrossfaderBackground
        }
    }

    DeckSide {
        id: rightDeck

        group: "[Channel2]"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: center.right
        anchors.right: parent.right
        anchors.margins: 8
    }
}
