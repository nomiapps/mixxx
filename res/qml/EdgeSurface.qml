import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Window 2.12
import "Theme"

// The Edge performance surface, laid out like a DDJ-FLX4: jog with PLAY/CUE
// stacked bottom-left, tempo fader + BEAT SYNC on the deck's right edge,
// performance pads below the jog, and a 2-channel club mixer in the center
// (TRIM / HI / MID / LOW / CFX, channel faders with headphone cue, crossfader).
// Sized against a 2560x720 canvas; everything scales as one locked unit.
Window {
    id: root

    width: 2560
    height: 720
    color: Theme.backgroundColor
    title: "Mixxx - Edge Surface"

    readonly property real ui: Math.min(width / 2560, height / 720)

    component KnobCell: Column {
        id: knobCell

        required property string knobGroup
        required property string knobKey
        required property string label
        property color knobColor: Theme.deckActiveColor

        spacing: 2 * root.ui

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: knobCell.label
            color: Theme.deckTextColor
            font.pixelSize: Math.max(9, 13 * root.ui)
        }

        Skin.ControlKnob {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 52 * root.ui
            height: 52 * root.ui
            group: knobCell.knobGroup
            key: knobCell.knobKey
            color: knobCell.knobColor
        }
    }

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
            font.pixelSize: Math.max(12, 20 * root.ui)
            elide: Text.ElideRight
            width: parent.width * 0.9
            horizontalAlignment: Text.AlignHCenter
        }

        // transport column, FLX4-style bottom-left: CUE above PLAY
        Column {
            id: transportColumn

            anchors.left: parent.left
            anchors.bottom: padsGrid.top
            anchors.bottomMargin: 14 * root.ui
            spacing: 8 * root.ui

            Skin.ControlButton {
                width: 120 * root.ui
                height: 64 * root.ui
                group: side.group
                key: "cue_default"
                text: "CUE"
                activeColor: Theme.deckActiveColor
            }

            Skin.ControlButton {
                width: 120 * root.ui
                height: 84 * root.ui
                group: side.group
                key: "play"
                text: "PLAY"
                toggleable: true
                activeColor: Theme.deckActiveColor
            }
        }

        // tempo fader + beat sync on the deck's right edge
        Column {
            id: tempoColumn

            anchors.right: parent.right
            anchors.top: trackText.bottom
            anchors.topMargin: 10 * root.ui
            spacing: 10 * root.ui

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "TEMPO"
                color: Theme.deckTextColor
                font.pixelSize: Math.max(9, 13 * root.ui)
            }

            Skin.ControlSlider {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 70 * root.ui
                height: 320 * root.ui
                orientation: Qt.Vertical
                group: side.group
                key: "rate"
                barColor: Theme.bpmSliderBarColor
                barStart: 0.5
                bg: Theme.imgBpmSliderBackground
            }

            Skin.ControlButton {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 100 * root.ui
                height: 44 * root.ui
                group: side.group
                key: "sync_enabled"
                text: "BEAT SYNC"
                toggleable: true
                activeColor: Theme.deckActiveColor
            }
        }

        Skin.EdgeDeckPlatter {
            id: platter

            group: side.group
            anchors.top: trackText.bottom
            anchors.bottom: padsGrid.top
            anchors.left: transportColumn.right
            anchors.right: tempoColumn.left
            anchors.margins: 6 * root.ui
        }

        Grid {
            id: padsGrid

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6 * root.ui
            anchors.horizontalCenter: parent.horizontalCenter
            columns: 4
            spacing: 6 * root.ui

            Repeater {
                model: 8

                Skin.HotcueButton {
                    required property int index

                    width: 110 * root.ui
                    height: 54 * root.ui
                    hotcueNumber: index + 1
                    group: side.group
                }
            }
        }
    }

    component MixerChannel: Item {
        id: channel

        required property string group

        Column {
            id: knobStack

            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6 * root.ui

            KnobCell {
                label: "TRIM"
                knobGroup: channel.group
                knobKey: "pregain"
                knobColor: Theme.white
            }

            KnobCell {
                label: "HI"
                knobGroup: "[EqualizerRack1_" + channel.group + "_Effect1]"
                knobKey: "parameter3"
            }

            KnobCell {
                label: "MID"
                knobGroup: "[EqualizerRack1_" + channel.group + "_Effect1]"
                knobKey: "parameter2"
            }

            KnobCell {
                label: "LOW"
                knobGroup: "[EqualizerRack1_" + channel.group + "_Effect1]"
                knobKey: "parameter1"
            }

            KnobCell {
                label: "CFX"
                knobGroup: "[QuickEffectRack1_" + channel.group + "]"
                knobKey: "super1"
                knobColor: Theme.crossfaderBarColor
            }
        }

        Skin.ControlSlider {
            id: volumeSlider

            anchors.top: knobStack.bottom
            anchors.topMargin: 8 * root.ui
            anchors.bottom: pflButton.top
            anchors.bottomMargin: 6 * root.ui
            anchors.horizontalCenter: parent.horizontalCenter
            width: 60 * root.ui
            orientation: Qt.Vertical
            group: channel.group
            key: "volume"
            barColor: Theme.volumeSliderBarColor
            bg: Theme.imgVolumeSliderBackground
        }

        Skin.ControlButton {
            id: pflButton

            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: 70 * root.ui
            height: 30 * root.ui
            group: channel.group
            key: "pfl"
            text: "CUE"
            toggleable: true
            activeColor: Theme.deckActiveColor
        }
    }

    DeckSide {
        id: leftDeck

        group: "[Channel1]"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: mixer.left
        anchors.margins: 8 * root.ui
    }

    Item {
        id: mixer

        width: 460 * root.ui
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8 * root.ui
        anchors.bottomMargin: 8 * root.ui

        MixerChannel {
            id: mixerChannel1

            group: "[Channel1]"
            width: 180 * root.ui
            anchors.top: parent.top
            anchors.bottom: crossfader.top
            anchors.bottomMargin: 10 * root.ui
            anchors.left: parent.left
            anchors.leftMargin: 20 * root.ui
        }

        MixerChannel {
            id: mixerChannel2

            group: "[Channel2]"
            width: 180 * root.ui
            anchors.top: parent.top
            anchors.bottom: crossfader.top
            anchors.bottomMargin: 10 * root.ui
            anchors.right: parent.right
            anchors.rightMargin: 20 * root.ui
        }

        Skin.ControlSlider {
            id: crossfader

            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width * 0.9
            height: 44 * root.ui
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
        anchors.left: mixer.right
        anchors.right: parent.right
        anchors.margins: 8 * root.ui
    }
}
