import "." as Skin
import QtQuick 2.12

Item {
    id: root

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")

    Skin.EdgeDeckPlatter {
        anchors.fill: parent
        group: root.groupResolved
    }
}
