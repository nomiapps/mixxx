import "." as Skin
import QtQuick 2.12

Item {
    id: root

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")

    Skin.VuMeter {
        anchors.fill: parent
        group: root.groupResolved
        key: root.spec.key ?? "vu_meter"
    }
}
