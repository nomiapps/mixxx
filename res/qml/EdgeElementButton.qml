pragma ComponentBehavior: Bound
import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    required property var spec
    // With spec.activeKey the button is momentary but lights from a separate
    // control (e.g. key reloop_toggle lit by loop_enabled).
    readonly property bool splitActive: (spec.activeKey ?? "") !== ""
    property var surface: null

    Loader {
        anchors.fill: parent
        sourceComponent: root.splitActive ? padButton : controlButton
    }
    Component {
        id: controlButton

        Skin.ControlButton {
            activeColor: root.spec.color ?? Theme.deckActiveColor
            group: root.groupResolved
            key: root.spec.key
            text: root.spec.label ?? ""
            toggleable: root.spec.toggle === true
        }
    }
    Component {
        id: padButton

        Skin.EdgePadButton {
            activeColor: root.spec.color ?? Theme.deckActiveColor
            activeGroup: root.surface ? root.surface.resolveGroup(root.spec.activeGroup ?? root.spec.group ?? "") : (root.spec.activeGroup ?? root.spec.group ?? "")
            activeKey: root.spec.activeKey
            padGroup: root.groupResolved
            padKey: root.spec.key
            text: root.spec.label ?? ""
        }
    }
}
