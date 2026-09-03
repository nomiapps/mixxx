pragma ComponentBehavior: Bound
import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    // With spec.activeKey the button is momentary but lights from a separate
    // control (e.g. key reloop_toggle lit by loop_enabled).
    readonly property bool splitActive: (spec.activeKey ?? "") !== ""

    Loader {
        anchors.fill: parent
        sourceComponent: root.splitActive ? padButton : controlButton
    }

    Component {
        id: controlButton

        Skin.ControlButton {
            group: root.groupResolved
            key: root.spec.key
            text: root.spec.label ?? ""
            toggleable: root.spec.toggle === true
            activeColor: root.spec.color ?? Theme.deckActiveColor
        }
    }

    Component {
        id: padButton

        Skin.EdgePadButton {
            padGroup: root.groupResolved
            padKey: root.spec.key
            activeGroup: root.surface ? root.surface.resolveGroup(root.spec.activeGroup ?? root.spec.group ?? "") : (root.spec.activeGroup ?? root.spec.group ?? "")
            activeKey: root.spec.activeKey
            text: root.spec.label ?? ""
            activeColor: root.spec.color ?? Theme.deckActiveColor
        }
    }
}
