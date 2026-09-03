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
    height: 42
    radius: 5
    width: 42

    TapHandler {
        onDoubleTapped: {
            statusControl.value = !statusControl.value;
        }
    }
    Skin.ControlKnob {
        id: knob

        anchors.centerIn: root
        height: 36
        // Dim only the knob when the band is killed so the kill button stays
        // fully legible.
        opacity: statusControl.value ? 0.4 : 1
        width: 36
    }
    Mixxx.ControlProxy {
        id: statusControl

        group: root.statusGroup
        key: root.statusKey
    }
    // Explicit kill button so the toggle reads as a button, not an indicator.
    // Small and round, tucked into the corner so it clears the knob. Red when
    // killed (a kill mutes the band) and on hover, to match the filter's red
    // power button.
    Rectangle {
        id: statusButton

        anchors.bottom: root.bottom
        anchors.bottomMargin: 2
        anchors.left: root.left
        anchors.leftMargin: 2
        border.color: statusControl.value || statusMouse.containsMouse ? Theme.red : Theme.buttonNormalColor
        border.width: 1
        color: statusControl.value || statusMouse.pressed ? Theme.red : (statusMouse.containsMouse ? Theme.darkGray2 : "transparent")
        height: 11
        radius: width / 2
        width: 11

        MouseArea {
            id: statusMouse

            // oversized hit target around the small button
            anchors.fill: parent
            anchors.margins: -3
            hoverEnabled: true

            onClicked: statusControl.value = !statusControl.value
        }
    }
}
