import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    property alias knob: volume

    required property string stemGroup
    required property string label
    required property color stemColor

    readonly property string fxGroup: `[QuickEffectRack1_${stemGroup}]`

    width: 56
    height: 72
    radius: 5
    color: stemColor
    opacity: statusControl.value ? 0.5 : 1

    Mixxx.ControlProxy {
        id: statusControl

        group: root.stemGroup
        key: "mute"
    }

    Mixxx.ControlProxy {
        id: fxControl

        group: root.fxGroup
        key: "enabled"
    }

    // top row: mute dot + label, nothing else in this band
    Rectangle {
        id: statusButton

        anchors.left: root.left
        anchors.top: root.top
        anchors.leftMargin: 3
        anchors.topMargin: 3
        width: 11
        height: width
        radius: width / 2
        border.width: 1
        border.color: Theme.buttonNormalColor
        color: statusControl.value ? volume.color : "transparent"

        TapHandler {
            onTapped: statusControl.value = !statusControl.value
        }
    }

    Text {
        id: stemLabel

        anchors.left: statusButton.right
        anchors.right: root.right
        anchors.top: root.top
        anchors.topMargin: 3
        anchors.leftMargin: 3
        anchors.rightMargin: 2
        elide: Text.ElideRight
        text: label
        font.pixelSize: 10
    }

    // middle row: volume knob left, quick-effect knob + its enable dot right
    Skin.ControlKnob {
        id: volume

        group: root.stemGroup
        key: "volume"
        color: Theme.gainKnobColor
        anchors.left: root.left
        anchors.top: stemLabel.bottom
        anchors.leftMargin: 2
        anchors.topMargin: 2
        arcStart: 0
        width: 30
        height: 30
    }

    Skin.ControlMiniKnob {
        id: effectSuperKnob

        anchors.right: root.right
        anchors.top: stemLabel.bottom
        anchors.rightMargin: 3
        anchors.topMargin: 4
        width: 18
        height: 18
        arcStart: Knob.ArcStart.Minimum
        group: root.fxGroup
        key: "super1"
        color: Theme.effectColor
        opacity: fxControl.value ? 1 : 0.5
    }

    Rectangle {
        id: fxButton

        anchors.horizontalCenter: effectSuperKnob.horizontalCenter
        anchors.top: effectSuperKnob.bottom
        anchors.topMargin: 3
        width: 11
        height: width
        radius: width / 2
        border.width: 1
        border.color: Theme.buttonNormalColor
        color: fxControl.value ? effectSuperKnob.color : "transparent"

        TapHandler {
            onTapped: fxControl.value = !fxControl.value
        }
    }

    Mixxx.ControlProxy {
        id: fxSelect

        group: root.fxGroup
        key: "loaded_chain_preset"
    }

    // bottom row: effect preset selector on its own band
    Skin.ComboBox {
        id: effectSelector

        anchors.left: root.left
        anchors.right: root.right
        anchors.bottom: root.bottom
        anchors.margins: 1
        height: 16
        spacing: 2
        indicator.width: 0
        popupWidth: 150
        clip: true
        opacity: fxControl.value ? 1 : 0.5
        textRole: "display"
        font.pixelSize: 10
        model: Mixxx.EffectsManager.quickChainPresetModel
        currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
        onActivated: (index) => {
            fxSelect.value = index;
        }
    }
}
