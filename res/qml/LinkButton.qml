import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import "Theme"
import "." as Skin

Skin.Button {
    id: root

    implicitWidth: 94
    activeColor: Theme.white
    checked: linkEnabled.value > 0
    enabled: linkEnabled.keyValid
    text: checked ? qsTr("Link · %1").arg(Math.round(peers.value)) : qsTr("Link")
    Accessible.name: qsTr("Ableton Link")
    Accessible.description: checked
        ? qsTr("Enabled, %1 connected peers").arg(Math.round(peers.value))
        : qsTr("Disabled")
    ToolTip.visible: hovered
    ToolTip.delay: 500
    ToolTip.text: checked
        ? qsTr("%1 Link peers. Enable deck Sync to share tempo and beat timing. Start the MPC on the desired downbeat.").arg(Math.round(peers.value))
        : qsTr("Join Ableton Link to share tempo and beat timing with the MPC.")

    onClicked: linkEnabled.toggle()

    Mixxx.ControlProxy {
        id: linkEnabled
        group: "[AbletonLink]"
        key: "sync_enabled"
    }
    Mixxx.ControlProxy {
        id: peers
        group: "[AbletonLink]"
        key: "num_peers"
    }
}
