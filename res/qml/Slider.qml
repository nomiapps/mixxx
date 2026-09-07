import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import "Theme"

RowLayout {
    id: root
    property list<var> markers: ["", ""]

    property alias suffix: textInputSection.suffix
    property alias slider: control
    property alias value: control.value

    height: 30

    Slider {
        id: control

        Layout.fillWidth: true

        background: Item {
            x: control.leftPadding + 7
            implicitWidth: 200
            implicitHeight: 4
            width: control.availableWidth - 7
            height: control.availableHeight
            // Track and fill, as the chrome draws a fader bar: a dark groove with
            // a Theme.blue line up to the handle.
            Rectangle {
                width: parent.width
                height: 4
                radius: 2
                color: Theme.darkGray
            }
            Rectangle {
                width: control.visualPosition * parent.width
                height: 4
                radius: 2
                color: Theme.blue
            }
            Repeater {
                id: delegate
                model: markers
                anchors.fill: parent
                anchors.leftMargin: 7
                Item {
                    required property int index
                    required property var modelData
                    x: parent.width * (index / (delegate.model.length - 1))
                    y: -4
                    height: control.availableHeight

                    Rectangle {
                        id: mark
                        visible: modelData != null
                        anchors {
                            top: parent.top
                        }
                        width: 1
                        height: 11
                        color: Qt.alpha(Theme.white, 0.25)
                    }
                    Text {
                        id: label
                        visible: modelData != null
                        anchors {
                            top: mark.bottom
                            topMargin: 4
                            horizontalCenter: mark.left
                        }
                        color: Qt.alpha(Theme.white, 0.25)
                        font.pixelSize: 10
                        text: modelData ?? ""
                    }
                }
            }
        }
        // A ring, not a glowing dot: window ground inside a 2px Theme.blue
        // hairline, tinted while held or hovered.
        handle: Rectangle {
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: -5
            width: 14
            height: 14
            radius: 7
            border.width: 2
            border.color: Theme.blue
            color: control.pressed || handleHover.hovered ? Theme.selectionColor : Theme.fieldBackgroundColor
            HoverHandler {
                id: handleHover
            }
        }
    }
    FocusScope {
        id: textInputSection
        Layout.leftMargin: 17
        Layout.minimumWidth: fontMetrics.advanceWidth + 8
        Layout.preferredHeight: 30
        Layout.margins: 4

        property string suffix: ""
        visible: suffix.length > 0

        // The value field, drawn as the settings shell draws its search field.
        Rectangle {
            id: backgroundInput
            radius: 4
            color: Theme.fieldBackgroundColor
            border.width: 1
            border.color: valueInput.activeFocus ? Theme.blue : Theme.panelBorderColor
            anchors.fill: parent
            anchors.margins: 4
        }
        Item {
            anchors.fill: parent
            anchors.margins: 4
            TextInput {
                id: valueInput
                anchors.left: parent.left
                anchors.right: inputField.left
                anchors.margins: 3
                focus: true
                color: Qt.alpha(acceptableInput ? Theme.white : Theme.warningColor, root.enabled ? 1 : 0.5)
                onAccepted: {
                    control.value = parseInt(text)
                }
                text: Math.round(control.value)
                horizontalAlignment: TextInput.AlignRight
                validator: IntValidator {bottom: control.from; top: control.to}
            }
            Text {
                id: inputField
                anchors.right: parent.right
                anchors.margins: textInputSection.suffix.length > 0 ? 10 : 0
                text: textInputSection.suffix
                color: Qt.alpha(Theme.white, root.enabled ? 1 : 0.5)
                TextMetrics  {
                    id: fontMetrics
                    font.family: inputField.font.family
                    text: `${control.to} ${parent.text}`
                }
            }
        }
    }
}
