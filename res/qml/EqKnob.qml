import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    property alias knob: knob
    property string statusGroup: root.knob.group
    required property string statusKey

    color: Theme.knobBackgroundColor
    width: 56
    height: 56
    radius: 5

    Skin.ControlKnob {
        id: knob

        anchors.centerIn: root
        width: 48
        height: 48
    }

    Mixxx.ControlProxy {
        id: statusControl

        group: root.statusGroup
        key: root.statusKey
    }

    Rectangle {
        id: statusButton

        // Small round button tucked into the corner so it clears the knob.
        anchors.left: root.left
        anchors.bottom: root.bottom
        anchors.leftMargin: 2
        anchors.bottomMargin: 2
        width: 11
        height: 11
        radius: width / 2
        border.width: 1
        // Red when killed (a kill mutes the band) and on hover, to match the
        // filter's red power button.
        border.color: statusControl.value
                ? Theme.red
                : (statusMouse.containsMouse ? Theme.red : Theme.buttonNormalColor)
        color: statusControl.value
                ? Theme.red
                : (statusMouse.pressed
                        ? Theme.red
                        : (statusMouse.containsMouse ? Theme.darkGray2 : "transparent"))

        MouseArea {
            id: statusMouse

            // oversized hit target around the small button
            anchors.fill: parent
            anchors.margins: -6
            hoverEnabled: true
            onClicked: statusControl.value = !statusControl.value
        }
    }
}
