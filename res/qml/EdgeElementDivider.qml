import QtQuick 2.12
import "Theme"

// Panel seam: a shadow line separating surface sections (e.g. the deck
// sections from the mixer), like the panel joints on hardware controllers.
// Vertical when taller than wide, horizontal otherwise. Non-interactive.
Item {
    id: root

    required property var spec
    property var surface: null
    readonly property bool vertical: height >= width

    // dark groove
    Rectangle {
        anchors.centerIn: parent
        width: root.vertical ? 3 : parent.width
        height: root.vertical ? parent.height : 3
        color: "#000000"
        opacity: 0.55
    }

    // lit edge beside the groove
    Rectangle {
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: root.vertical ? 2 : 0
        anchors.verticalCenterOffset: root.vertical ? 0 : 2
        width: root.vertical ? 1 : parent.width
        height: root.vertical ? parent.height : 1
        color: Theme.pureWhite
        opacity: 0.08
    }
}
