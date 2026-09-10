import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec
    // Unused here, but the surface hands every element a reference to itself
    // when it loads one; without the property the assignment is an error and
    // the element is the only one in the set that logs on every layout load.
    property var surface: null

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
