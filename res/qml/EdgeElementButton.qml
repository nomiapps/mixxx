import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")

    Skin.ControlButton {
        anchors.fill: parent
        group: root.groupResolved
        key: root.spec.key
        text: root.spec.label ?? ""
        toggleable: root.spec.toggle === true
        activeColor: root.spec.color ?? Theme.deckActiveColor
    }
}
