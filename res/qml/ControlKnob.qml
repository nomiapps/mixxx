import "." as Skin
import "AccessibleNames.js" as AccessibleNames
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Skin.Knob {
    id: root

    property alias group: control.group
    property alias key: control.key

    Accessible.name: AccessibleNames.forControl(root.group, root.key)
    value: control.parameter

    onTurned: control.parameter = value

    Mixxx.ControlProxy {
        id: control
    }
    TapHandler {
        onDoubleTapped: control.reset()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton

        onTapped: control.reset()
    }
}
