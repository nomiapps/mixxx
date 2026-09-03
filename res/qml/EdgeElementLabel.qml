import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec

    Text {
        anchors.fill: parent
        text: root.spec.label ?? ""
        color: root.spec.color ?? Theme.deckTextColor
        font.pixelSize: Math.max(9, root.height * 0.7)
        font.bold: true
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
