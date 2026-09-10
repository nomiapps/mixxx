pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

Item {
    id: root

    // Sizes are properties rather than literals so the same slot can be drawn
    // at touch size on the Edge surface. The defaults are the desktop values
    // this file has always used, so the main window is unchanged.
    property int cellWidth: 55
    required property int effectNumber
    readonly property string effectUnitGroup: slot.chainSlotGroup
    property bool expanded: false
    readonly property string group: slot.group
    property int focusButtonSize: 16
    property int knobSize: 30
    property real maxSelectorWidth: 300
    readonly property int maximumParametersPerType: 8
    property int parameterButtonHeight: 22
    property int selectorControlWidth: 40
    // The per-parameter LINK / invert row. Desktop power-user controls at 7 px:
    // a touch surface turns them off rather than shrinking to nothing.
    property bool showParameterLinkControls: true
    property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(unitNumber, effectNumber)
    required property int unitNumber

    height: 50

    Item {
        id: selector

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        width: Math.min(root.width, root.maxSelectorWidth)

        EffectFocusButton {
            id: focusButton

            anchors.left: parent.left
            anchors.margins: 5
            anchors.verticalCenter: parent.verticalCenter
            effectNumber: root.effectNumber
            effectUnitGroup: root.effectUnitGroup
            height: root.focusButtonSize
            width: visible ? root.focusButtonSize : 0
        }
        Skin.ControlButton {
            id: effectEnableButton

            activeColor: Theme.effectColor
            anchors.bottom: parent.bottom
            anchors.left: focusButton.right
            anchors.margins: 5
            anchors.top: parent.top
            group: root.group
            key: "enabled"
            text: "ON"
            toggleable: true
            width: root.selectorControlWidth
        }
        EffectSelector {
            id: effectSelector

            anchors.bottom: parent.bottom
            anchors.left: effectEnableButton.right
            anchors.margins: 5
            anchors.right: effectMetaKnob.left
            anchors.top: parent.top
            slot: root.slot
        }
        Skin.ControlMiniKnob {
            id: effectMetaKnob

            anchors.bottom: parent.bottom
            anchors.margins: 5
            anchors.right: parent.right
            anchors.top: parent.top
            arcStart: Skin.Knob.ArcStart.Minimum
            color: Theme.effectColor
            group: root.group
            key: "meta"
            width: root.selectorControlWidth
        }
    }
    ListView {
        id: parametersView

        anchors.bottom: parent.bottom
        anchors.left: selector.right
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.top: parent.top
        clip: true
        model: root.slot.parametersModel
        orientation: ListView.Horizontal
        spacing: 5
        visible: root.expanded

        delegate: Item {
            id: parameter

            required property string controlKey
            required property int index
            readonly property bool isButton: type === Mixxx.EffectSlotParametersModel.Button
            readonly property bool isKnob: type === Mixxx.EffectSlotParametersModel.Knob
            readonly property string label: shortName || name
            required property bool loaded
            required property string name
            required property string shortName
            readonly property bool shown: loaded && slotNumber > 0 && slotNumber <= root.maximumParametersPerType
            readonly property int slotNumber: {
                const match = controlKey.match(/(\d+)$/);
                return match ? Number(match[1]) : 0;
            }
            required property int type

            height: parametersView.height
            visible: shown
            width: shown ? root.cellWidth : 0

            Skin.EmbeddedText {
                anchors.fill: parent
                font.bold: false
                text: parameter.label
                verticalAlignment: Text.AlignBottom
            }
            Loader {
                active: parameter.shown && parameter.isKnob
                anchors.horizontalCenter: parent.horizontalCenter
                height: root.knobSize
                width: root.knobSize

                sourceComponent: Skin.ControlMiniKnob {
                    arcStart: 0
                    color: Theme.effectColor
                    group: root.group
                    key: parameter.controlKey
                }
            }
            Loader {
                active: parameter.shown && parameter.isButton
                anchors.horizontalCenter: parent.horizontalCenter
                height: root.parameterButtonHeight
                width: parent.width

                sourceComponent: Skin.ControlButton {
                    activeColor: Theme.effectColor
                    group: root.group
                    key: parameter.controlKey
                    text: "ON"
                    toggleable: true
                }
            }
            Row {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                height: 8
                visible: parameter.shown && parameter.isKnob && root.showParameterLinkControls

                Skin.ControlButton {
                    activeColor: Theme.effectColor
                    group: root.group
                    height: 8
                    key: parameter.controlKey + "_link_inverse"
                    text: "I"
                    toggleable: true
                    width: 12
                }
                Item {
                    height: 8
                    width: 34

                    Rectangle {
                        anchors.fill: parent
                        color: linkTypeControl.value > 0 ? Theme.effectColor : Theme.knobBackgroundColor
                    }
                    Text {
                        anchors.centerIn: parent
                        font.pixelSize: 7
                        text: "LINK " + Math.round(linkTypeControl.value)
                    }
                    TapHandler {
                        onTapped: linkTypeControl.value = (Math.round(linkTypeControl.value) + 1) % 5
                    }
                    Mixxx.ControlProxy {
                        id: linkTypeControl

                        group: root.group
                        key: parameter.controlKey + "_link_type"
                    }
                }
            }
        }
    }
}
