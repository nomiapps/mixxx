import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    required property string group
    property alias knob: knob
    // Mixer filter shows the preset dropdown in this cell. Deck stem FX
    // knobs already have a selector beside them, so they turn this off.
    property bool showPreset: true

    color: Theme.knobBackgroundColor
    height: showPreset ? 52 : 46
    radius: 5
    width: 42

    Skin.ControlKnob {
        id: knob

        anchors.bottom: root.showPreset ? undefined : root.bottom
        anchors.bottomMargin: root.showPreset ? 0 : 2
        anchors.horizontalCenter: root.horizontalCenter
        anchors.top: root.showPreset ? root.top : undefined
        anchors.topMargin: root.showPreset ? 1 : 0
        group: root.group
        height: root.showPreset ? 30 : Math.max(16, Math.min(root.width - 4, root.height - statusButton.height - 6))
        key: "super1"
        // Dim only the knob when the effect is off so the power button stays
        // fully legible.
        opacity: statusControl.value ? 1 : 0.4
        width: height
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
    Mixxx.ControlProxy {
        id: fxSelect

        group: root.group
        key: "loaded_chain_preset"
    }
    // Explicit power button so the toggle reads as an on/off button, not an
    // indicator LED. Placed beneath the knob so it never covers the control;
    // lit in the knob's color when the effect is on and on hover.
    Rectangle {
        id: statusButton

        anchors.bottom: root.showPreset ? root.bottom : knob.top
        anchors.bottomMargin: 2
        anchors.left: root.showPreset ? root.left : undefined
        anchors.leftMargin: root.showPreset ? 3 : 0
        anchors.horizontalCenter: root.showPreset ? undefined : root.horizontalCenter
        anchors.right: undefined
        anchors.rightMargin: 0
        border.color: statusControl.value || statusMouse.containsMouse ? knob.color : Theme.buttonNormalColor
        border.width: 1
        color: statusControl.value || statusMouse.pressed ? knob.color : (statusMouse.containsMouse ? Theme.darkGray2 : "transparent")
        height: showPreset ? 16 : 10
        radius: 3
        width: showPreset ? 12 : 10

        Text {
            anchors.centerIn: parent
            color: statusControl.value ? Theme.knobBackgroundColor : Theme.buttonNormalColor
            font.family: Theme.fontFamily
            font.pixelSize: showPreset ? 8 : 7
            text: "⏻"
        }
        MouseArea {
            id: statusMouse

            anchors.fill: parent
            anchors.margins: -3
            hoverEnabled: true

            onClicked: statusControl.value = !statusControl.value
        }
    }
    Skin.OpaqueComboBox {
        id: effectSelector

        anchors.bottom: root.bottom
        anchors.bottomMargin: 2
        anchors.left: statusButton.right
        anchors.leftMargin: 2
        anchors.right: root.right
        anchors.rightMargin: 2
        clip: true
        currentIndex: fxSelect.value === -1 ? 0 : fxSelect.value
        font.family: Theme.fontFamily
        font.pixelSize: 8
        height: 16
        model: Mixxx.EffectsManager.quickChainPresetModel
        opacity: statusControl.value ? 1 : 0.85
        popupWidth: 150
        showIndicator: false
        textRole: "display"
        visible: root.showPreset

        onActivated: index => {
            fxSelect.value = index;
        }
    }
    Text {
        anchors.bottom: root.bottom
        anchors.bottomMargin: 2
        anchors.left: statusButton.right
        anchors.leftMargin: 2
        anchors.right: root.right
        anchors.rightMargin: 2
        color: statusControl.value ? Theme.eqFxColor : Theme.lightGray3
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.pixelSize: Theme.buttonFontPixelSize
        height: statusButton.height
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("FILTER")
        verticalAlignment: Text.AlignVCenter
        visible: false
    }
}
