import "." as Skin
import "AccessibleNames.js" as AccessibleNames
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Skin.Fader {
    id: root

    required property string group
    required property string key

    // Edge.Controls.Slider supplies the role and the value; this supplies the
    // name, which nothing else can know.
    Accessible.name: AccessibleNames.forControl(root.group, root.key)
    value: control.parameter

    onMoved: function (value) {
        control.parameter = value;
    }

    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
    }
    TapHandler {
        onDoubleTapped: control.reset()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton

        onTapped: control.reset()
    }
}
