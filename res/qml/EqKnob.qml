import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    property alias knob: knob
    property string labelText: ""
    property string statusGroup: root.knob.group
    required property string statusKey

    color: Theme.knobBackgroundColor
    height: 46
    radius: 5
    width: 42

    TapHandler {
        onDoubleTapped: {
            statusControl.value = !statusControl.value;
        }
    }
    Skin.ControlKnob {
        id: knob

        anchors.horizontalCenter: root.horizontalCenter
        anchors.top: root.top
        anchors.topMargin: 1
        height: 32
        // Dim only the knob when the band is killed so the kill button stays
        // fully legible.
        opacity: statusControl.value ? 0.4 : 1
        width: 32
    }
    Mixxx.ControlProxy {
        id: statusControl

        group: root.statusGroup
        key: root.statusKey
    }
    // Explicit kill button so the toggle reads as a button, not an indicator.
    // Small and round, placed beneath the knob so it never covers the control.
    // Red when
    // killed (a kill mutes the band) and on hover, to match the filter's red
    // power button.
    Rectangle {
        id: statusButton

        anchors.bottom: root.bottom
        anchors.bottomMargin: 2
        anchors.left: root.left
        anchors.leftMargin: 4
        border.color: statusControl.value || statusMouse.containsMouse ? Theme.red : Theme.buttonNormalColor
        border.width: 1
        color: statusControl.value || statusMouse.pressed ? Theme.red : (statusMouse.containsMouse ? Theme.darkGray2 : "transparent")
        height: 10
        radius: width / 2
        width: 10

        MouseArea {
            id: statusMouse

            // oversized hit target around the small button
            anchors.fill: parent
            anchors.margins: -3
            hoverEnabled: true

            onClicked: statusControl.value = !statusControl.value
        }
    }
    Text {
        anchors.bottom: root.bottom
        anchors.bottomMargin: 2
        anchors.left: statusButton.right
        anchors.leftMargin: 2
        anchors.right: root.right
        anchors.rightMargin: 2
        color: statusControl.value ? Theme.red : Theme.lightGray3
        font.bold: true
        font.pixelSize: 7
        height: statusButton.height
        horizontalAlignment: Text.AlignHCenter
        text: root.labelText
        verticalAlignment: Text.AlignVCenter
    }
}
