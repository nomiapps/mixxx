import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec

    Skin.ControlButton {
        anchors.fill: parent
        group: root.spec.group
        key: root.spec.key
        text: root.spec.label ?? ""
        toggleable: root.spec.toggle === true
        activeColor: root.spec.color ?? Theme.deckActiveColor
    }
}
