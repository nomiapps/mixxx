import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group

    Rectangle {
        anchors.fill: parent
        border.color: "#343438"
        border.width: 1
        color: "#171719"
        radius: 5
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.accentColor
        height: 2
        radius: 1
        z: 2
    }

    Rectangle {
        id: gainKnobFrame

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        border.color: "#3d3d42"
        border.width: 1
        color: Theme.knobBackgroundColor
        height: width
        radius: 5

        Skin.ControlKnob {
            id: gainKnob

            anchors.centerIn: parent
            color: Theme.gainKnobColor
            group: root.group
            height: 36
            key: "pregain"
            width: 36
        }
    }
    Item {
        id: levelSection

        anchors.bottom: pflButton.top
        anchors.bottomMargin: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: gainKnobFrame.bottom
        anchors.topMargin: 5

        Rectangle {
            anchors.fill: parent
            border.color: "#2e2e32"
            border.width: 1
            color: "#111113"
            radius: 4
        }
        Rectangle {
            id: leftMeterWell

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 7
            anchors.left: parent.left
            anchors.leftMargin: 5
            anchors.top: parent.top
            anchors.topMargin: 7
            color: "#070708"
            radius: width / 2
            width: 8

            Skin.VuMeter {
                anchors.fill: parent
                anchors.margins: 1
                group: root.group
                key: "vu_meter_left"
            }
        }
        Rectangle {
            id: rightMeterWell

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 7
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.top: parent.top
            anchors.topMargin: 7
            color: "#070708"
            radius: width / 2
            width: 8

            Skin.VuMeter {
                anchors.fill: parent
                anchors.margins: 1
                group: root.group
                key: "vu_meter_right"
            }
        }
        Skin.ControlFader {
            id: volumeSlider

            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            bar.color: Theme.volumeSliderBarColor
            bg: Theme.imgVolumeSliderBackground
            group: root.group
            key: "volume"
            width: parent.width - 14

            handleImage {
                width: parent.width - 6
            }
        }
    }
    Skin.ControlButton {
        id: pflButton

        activeColor: Theme.pflActiveButtonColor
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        group: root.group
        key: "pfl"
        text: "PFL"
        toggleable: true
    }
}
