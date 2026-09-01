import "." as Skin
import QtQuick 2.12

Item {
    id: root

    required property var spec

    Skin.VuMeter {
        anchors.fill: parent
        group: root.spec.group
        key: root.spec.key ?? "vu_meter"
    }
}
