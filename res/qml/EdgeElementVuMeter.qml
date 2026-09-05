import "." as Skin
import QtQuick 2.12

Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    required property var spec
    property var surface: null

    Skin.VuMeter {
        anchors.fill: parent
        group: root.groupResolved
        key: root.spec.key ?? "vu_meter"
    }
}
