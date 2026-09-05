import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    required property var spec
    property var surface: null

    Text {
        id: label

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        color: Theme.deckTextColor
        font.pixelSize: Math.max(9, root.height * 0.18)
        text: root.spec.label ?? ""
    }
    Skin.ControlKnob {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: label.bottom
        anchors.topMargin: 2
        color: root.spec.color ?? Theme.deckActiveColor
        group: root.groupResolved
        height: width
        key: root.spec.key
        width: Math.min(root.width, root.height * 0.75)
    }
}
