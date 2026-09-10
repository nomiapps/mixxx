import "." as Skin
import "AccessibleNames.js" as AccessibleNames
import QtQuick 2.12

Skin.Button {
    id: root

    required property string group
    required property string key
    property bool toggleable: false

    function toggle() {
        controlBehavior.toggleControl();
    }

    Accessible.checkable: root.toggleable
    Accessible.checked: root.highlight

    // Most of these carry an icon rather than a label, so there is no text for
    // Qt to name them with -- but the control they are bound to already says
    // what they do. A call site with a better name still sets accessibleName.
    accessibleName: root.text ? root.text : AccessibleNames.forControl(root.group, root.key)
    highlight: controlBehavior.isActive

    onPressed: {
        controlBehavior.pressPrimary();
    }
    onReleased: {
        controlBehavior.releasePrimary();
    }

    ControlProxyButtonBehavior {
        id: controlBehavior

        group: root.group
        handlePointerInput: false
        key: root.key
        toggleable: root.toggleable
    }
}
