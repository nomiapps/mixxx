import QtQuick 2.12
import ".." as Skin
import "../Theme"

Skin.ControlButton {
    id: root

    property bool minimized: false

    activeColor: Theme.yellow
    compact: root.minimized
    key: "cue_default"
    text: qsTr("Cue")
    glyph: Rectangle {
        color: root.faceColor
        implicitHeight: 20
        implicitWidth: 20
        radius: width / 2
    }
}
