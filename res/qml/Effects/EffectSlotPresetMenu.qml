pragma ComponentBehavior: Bound

import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    property color backgroundColor: "#151517"
    property color borderColor: "#333333"
    property url checkedSource
    property color disabledTextColor: "#494949"
    property string fontFamily: "Open Sans"
    property int fontPixelSize: 14
    property color hoverColor: "#2c454f"
    property color hoverTextColor: "#ffffff"
    property color separatorBottomColor: "#333333"
    property color separatorTopColor: "#000000"
    required property Mixxx.EffectSlotProxy slot
    property url submenuArrowSource
    property color textColor: "#c2b3a5"

    bottomPadding: 5
    enabled: slot.loaded
    leftPadding: 5
    rightPadding: 5
    title: slot.number + ": " + slot.effectName
    topPadding: 5
    width: 160

    background: Rectangle {
        border.color: root.borderColor
        border.width: 1
        color: root.backgroundColor
        radius: 1
    }
    delegate: EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        textColor: root.textColor
    }

    // Instantiator does not promise that its objects arrive in model order, and
    // Menu.insertItem() clamps an index that is past the end -- so inserting at
    // the index the Instantiator reports can leave the items permanently
    // shuffled. The deck menu came out 1, 4, 2, 3 that way. Place each item
    // after the ones that sort before it, which also keeps instantiated items
    // above any static entries below them.
    function insertMenuItemInOrder(menu, item) {
        let pos = 0;
        while (pos < menu.count) {
            const at = menu.itemAt(pos);
            if (at === null || at.menuIndex === undefined || at.menuIndex >= item.menuIndex)
                break;
            pos++;
        }
        menu.insertItem(pos, item);
    }

    Instantiator {
        model: root.slot.parametersModel

        delegate: EffectPresetMenuItem {
            required property int index
            required property bool loaded
            required property string name
            required property string parameterId

            readonly property int menuIndex: index

            checkable: true
            checked: loaded
            checkedSource: root.checkedSource
            disabledTextColor: root.disabledTextColor
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            hoverColor: root.hoverColor
            hoverTextColor: root.hoverTextColor
            submenuArrowSource: root.submenuArrowSource
            text: name
            textColor: root.textColor

            onTriggered: root.slot.setParameterVisible(parameterId, !loaded)
        }

        onObjectAdded: (index, object) => root.insertMenuItemInOrder(root, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    EffectPresetMenuSeparator {
        bottomColor: root.separatorBottomColor
        topColor: root.separatorTopColor
    }
    EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        text: qsTr("Save snapshot")
        textColor: root.textColor

        onTriggered: root.slot.saveDefaultSnapshot()
    }
}
