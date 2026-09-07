import "." as Skin
import QtQuick 2.12

// The whole New UI library browser as a surface element: source tree, search,
// smart crates and track list. Self-contained -- it builds its own sidebar from
// the Mixxx.Library singleton -- so it takes no group and ignores @left/@right.
// Loading to a deck goes through the library's own double-click / load buttons.
Item {
    id: root

    required property var spec
    property var surface: null

    Skin.Library {
        anchors.fill: parent
        clip: true
    }

    // Inset frame, matching the other surface elements.
    Rectangle {
        anchors.fill: parent
        border.color: "#000000"
        border.width: 1
        color: "transparent"
        opacity: 0.5
    }
}
