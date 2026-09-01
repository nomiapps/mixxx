import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// A generic performance pad: pressing writes 1 to (padGroup, padKey), releasing
// writes 0 (Mixxx triggers fire on the rising edge; momentary controls get a
// proper press/release). Lights up from (activeGroup, activeKey).
Skin.Button {
    id: root

    required property string padGroup
    required property string padKey
    property string activeGroup: padGroup
    property string activeKey: padKey

    highlight: activeControl.value != 0
    onPressed: control.value = 1
    onReleased: control.value = 0

    Mixxx.ControlProxy {
        id: control

        group: root.padGroup
        key: root.padKey
    }

    Mixxx.ControlProxy {
        id: activeControl

        group: root.activeGroup
        key: root.activeKey
    }
}
