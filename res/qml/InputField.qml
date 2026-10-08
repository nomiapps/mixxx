import QtQuick
import QtQuick.Controls 2.15
import "Theme"

FocusScope {
    id: root

    property alias input: inputField

    Rectangle {
        id: backgroundInput
        radius: 4
        color: '#232323'
        border.color: Theme.panelBorderColor
        border.width: 1
        anchors.fill: parent
    }
    TextInput {
        id: inputField
        anchors.fill: parent
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 7
        focus: true
        clip: true
        color: acceptableInput ? "#FFFFFF" : "#7D3B3B"
        horizontalAlignment: TextInput.AlignLeft
    }
}
