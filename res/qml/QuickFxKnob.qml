import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

Rectangle {
    id: root

    property alias knob: knob
    required property string group

    color: Theme.knobBackgroundColor
    width: 56
    height: 56
    radius: 5

    Skin.ControlKnob {
        id: knob

        group: root.group
        key: "super1"

        anchors.horizontalCenter: root.horizontalCenter
        anchors.top: root.top
        width: 40
        height: 40
    }

    Mixxx.ControlProxy {
        id: statusControl

        group: root.group
        key: "enabled"
    }

    Rectangle {
        id: statusButton

        anchors.left: root.left
        anchors.bottom: root.bottom
        anchors.leftMargin: 4
        anchors.bottomMargin: 3
        width: 14
        height: 14
        radius: 3
        border.width: 1
        border.color: statusControl.value
                ? knob.color
                : (statusMouse.containsMouse ? knob.color : Theme.buttonNormalColor)
        color: statusControl.value
                ? knob.color
                : (statusMouse.pressed
                        ? knob.color
                        : (statusMouse.containsMouse ? Theme.darkGray2 : "transparent"))

        // Power glyph so it reads as an on/off button, not an indicator LED.
        Text {
            anchors.centerIn: parent
            text: "⏻"
            font.pixelSize: 10
            color: statusControl.value ? Theme.knobBackgroundColor : Theme.buttonNormalColor
        }

        MouseArea {
            id: statusMouse

            anchors.fill: parent
            anchors.margins: -3
            hoverEnabled: true
            onClicked: statusControl.value = !statusControl.value
        }
    }

    Mixxx.ControlProxy {
        id: fxSelect

        group: root.group
        key: "loaded_chain_preset"
    }

    // Opaque themed dropdown. Skin.ComboBox uses the semi-transparent
    // "embedded" background (and its popupWidth alias blocks overriding just
    // the popup), so this one is a plain ComboBox styled opaque locally.
    ComboBox {
        id: effectSelector
        anchors.left: statusButton.right
        anchors.leftMargin: 3
        anchors.right: root.right
        anchors.top: knob.bottom
        anchors.margins: 1
        clip: true

        // Only slightly dimmed when the effect is off — still clearly legible.
        opacity: statusControl.value ? 1 : 0.85
        textRole: "display"
        font.pixelSize: 10
        model: Mixxx.EffectsManager.quickChainPresetModel
        currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
        onActivated: (index) => {
            fxSelect.value = index
        }

        background: Rectangle {
            color: Theme.knobBackgroundColor
            radius: 3
            border.width: 1
            border.color: (effectSelector.pressed || effectSelector.popup.visible)
                    ? Theme.blue
                    : (effectSelector.hovered ? Theme.deckTextColor : Theme.midGray)
        }

        contentItem: Text {
            leftPadding: 5
            rightPadding: 4
            text: effectSelector.displayText
            color: Theme.deckTextColor
            font: effectSelector.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            clip: true
        }

        // No room for a chevron in this narrow cell; the bordered box (which
        // highlights on hover / when open) is the dropdown affordance.
        indicator: Item {
            width: 0
        }

        delegate: ItemDelegate {
            id: fxItem

            required property int index
            width: ListView.view ? ListView.view.width : effectSelector.width
            highlighted: effectSelector.highlightedIndex === fxItem.index

            contentItem: Text {
                text: effectSelector.textAt(fxItem.index)
                color: Theme.deckTextColor
                font.pixelSize: 11
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: fxItem.highlighted ? Qt.rgba(0.004, 0.863, 0.988, 0.18) : "transparent"
            }
        }

        popup: Popup {
            y: effectSelector.height + 2
            width: 150
            implicitHeight: Math.min(contentItem.implicitHeight, 300)
            padding: 4

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: effectSelector.popup.visible ? effectSelector.delegateModel : null
                currentIndex: effectSelector.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
            background: Rectangle {
                color: Theme.darkGray2
                radius: 4
                border.color: Theme.midGray
                border.width: 1
            }
        }
    }
}
