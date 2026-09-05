import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec

    Text {
        anchors.fill: parent
        color: root.spec.color ?? Theme.deckTextColor
        elide: Text.ElideRight
        font.bold: true
        font.pixelSize: Math.max(9, root.height * 0.7)
        horizontalAlignment: Text.AlignHCenter
        text: root.spec.label ?? ""
        verticalAlignment: Text.AlignVCenter
    }
}
