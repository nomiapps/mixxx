import "." as Skin
import QtQuick 2.12

Item {
    id: root

    required property var spec

    readonly property int rows: spec.rows ?? 2
    readonly property int columns: spec.columns ?? 4
    readonly property real gap: Math.max(3, height * 0.05)

    Grid {
        anchors.centerIn: parent
        columns: root.columns
        spacing: root.gap

        Repeater {
            model: root.rows * root.columns

            Skin.HotcueButton {
                required property int index

                width: (root.width - root.gap * (root.columns - 1)) / root.columns
                height: (root.height - root.gap * (root.rows - 1)) / root.rows
                hotcueNumber: index + 1
                group: root.spec.group
            }
        }
    }
}
