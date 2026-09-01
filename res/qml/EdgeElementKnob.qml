import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec

    Text {
        id: label

        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.spec.label ?? ""
        color: Theme.deckTextColor
        font.pixelSize: Math.max(9, root.height * 0.18)
    }

    Skin.ControlKnob {
        anchors.top: label.bottom
        anchors.topMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(root.width, root.height * 0.75)
        height: width
        group: root.spec.group
        key: root.spec.key
        color: root.spec.color ?? Theme.deckActiveColor
    }
}
