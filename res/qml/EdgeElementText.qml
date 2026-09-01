import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec

    property var player: Mixxx.PlayerManager.getPlayer(spec.group)

    Text {
        anchors.fill: parent
        text: root.player ? (root.player.artist + " - " + root.player.title) : ""
        color: Theme.deckTextColor
        font.pixelSize: Math.max(10, root.height * 0.7)
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
