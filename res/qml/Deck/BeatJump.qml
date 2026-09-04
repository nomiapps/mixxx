import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

Rectangle {
    id: root

    required property string group

    color: Theme.darkGray
    radius: 4

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 4
        color: Theme.lightGray3
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.pixelSize: Theme.buttonFontPixelSize
        text: qsTr("Beatjump")
    }
    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    BeatSizeSpinBoxBehavior {
        id: beatjumpSize

        decrementKey: "beatjump_size_halve"
        group: root.group
        incrementKey: "beatjump_size_double"
        key: "beatjump_size"
    }
    Skin.ControlButton {
        id: jumpBackButton

        activeColor: Theme.deckActiveColor
        enabled: trackLoadedControl.value > 0
        group: root.group
        implicitHeight: 26
        implicitWidth: 50
        key: "beatjump_backward"
        glyph: Item {
            implicitHeight: 20
            implicitWidth: 20

            ChevronGlyph {
                anchors.centerIn: parent
                doubled: true
                fillColor: jumpBackButton.faceColor
                forward: false
            }
        }

        width: (root.width - 18) / 2

        anchors {
            left: parent.left
            leftMargin: 6
            top: parent.top
            topMargin: 22
        }
    }
    Skin.ControlButton {
        id: jumpForwardButton

        activeColor: Theme.deckActiveColor
        enabled: trackLoadedControl.value > 0
        group: root.group
        implicitHeight: 26
        implicitWidth: 50
        key: "beatjump_forward"
        width: jumpBackButton.width
        glyph: Item {
            implicitHeight: 20
            implicitWidth: 20

            ChevronGlyph {
                anchors.centerIn: parent
                doubled: true
                fillColor: jumpForwardButton.faceColor
                forward: true
            }
        }

        anchors {
            right: parent.right
            rightMargin: 6
            top: parent.top
            topMargin: 22
        }
    }
    Skin.Button {
        id: jumpSizeHalfButton

        activeColor: Theme.deckActiveColor
        enabled: trackLoadedControl.value > 0
        implicitHeight: 28
        implicitWidth: 22
        glyph: Item {
            implicitHeight: 20
            implicitWidth: 20

            ChevronGlyph {
                anchors.centerIn: parent
                fillColor: jumpSizeHalfButton.faceColor
                forward: false
            }
        }

        onPressed: beatjumpSize.step(-1)

        anchors {
            bottom: parent.bottom
            bottomMargin: 7
            left: parent.left
            leftMargin: 6
        }
    }
    Item {
        implicitHeight: 28

        anchors {
            bottom: parent.bottom
            bottomMargin: 7
            left: jumpSizeHalfButton.right
            leftMargin: 6
            right: jumpSizeDoubleButton.left
            rightMargin: 6
        }

        BorderImage {
            id: sizeFace

            anchors.fill: parent
            horizontalTileMode: BorderImage.Stretch
            source: Theme.imgButton
            verticalTileMode: BorderImage.Stretch

            border {
                bottom: 10
                left: 10
                right: 10
                top: 10
            }
        }
        TextInput {
            id: sizeInput

            anchors.fill: parent
            color: trackLoadedControl.value > 0 ? Theme.buttonActiveColor : Theme.buttonDisableColor
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.buttonFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            text: beatjumpSize.valueText
            verticalAlignment: Text.AlignVCenter

            onAccepted: {
                beatjumpSize.commitText(sizeInput.text);
                sizeInput.focus = false;
                sizeInput.text = Qt.binding(function () {
                    return beatjumpSize.valueText;
                });
            }
        }
    }
    Skin.Button {
        id: jumpSizeDoubleButton

        activeColor: Theme.deckActiveColor
        enabled: trackLoadedControl.value > 0
        implicitHeight: 28
        implicitWidth: 22
        glyph: Item {
            implicitHeight: 20
            implicitWidth: 20

            ChevronGlyph {
                anchors.centerIn: parent
                fillColor: jumpSizeDoubleButton.faceColor
                forward: true
            }
        }

        onPressed: beatjumpSize.step(1)

        anchors {
            bottom: parent.bottom
            bottomMargin: 7
            right: parent.right
            rightMargin: 6
        }
    }
}
