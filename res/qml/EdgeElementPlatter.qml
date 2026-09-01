import "." as Skin
import QtQuick 2.12

Item {
    id: root

    required property var spec

    Skin.EdgeDeckPlatter {
        anchors.fill: parent
        group: root.spec.group
    }
}
