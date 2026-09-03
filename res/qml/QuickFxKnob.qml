import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    required property string group
    property alias knob: knob

    color: Theme.knobBackgroundColor
    height: 42
    radius: 5
    width: 42

    Skin.ControlKnob {
        id: knob

        anchors.horizontalCenter: root.horizontalCenter
        anchors.top: root.top
        group: root.group
        height: 36
        key: "super1"
        // Dim only the knob when the effect is off so the power button stays
        // fully legible.
        opacity: statusControl.value ? 1 : 0.4
        width: 36
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton

        onDoubleTapped: {
            statusControl.value = !statusControl.value;
        }
    }
    Mixxx.ControlProxy {
        id: statusControl

        group: root.group
        key: "enabled"
    }
    // Explicit power button so the toggle reads as an on/off button, not an
    // indicator LED. Tucked into the corner so it clears the knob; lit in the
    // knob's color when the effect is on and on hover.
    Rectangle {
        id: statusButton

        anchors.bottom: root.bottom
        anchors.bottomMargin: 2
        anchors.left: root.left
        anchors.leftMargin: 2
        border.color: statusControl.value || statusMouse.containsMouse ? knob.color : Theme.buttonNormalColor
        border.width: 1
        color: statusControl.value || statusMouse.pressed ? knob.color : (statusMouse.containsMouse ? Theme.darkGray2 : "transparent")
        height: 12
        radius: 3
        width: 12

        // Power glyph
        Text {
            anchors.centerIn: parent
            color: statusControl.value ? Theme.knobBackgroundColor : Theme.buttonNormalColor
            font.pixelSize: 8
            text: "⏻"
        }
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
