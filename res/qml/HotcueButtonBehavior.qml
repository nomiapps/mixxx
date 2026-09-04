import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Skin.ControlProxyButtonBehavior {
    id: root

    required property int hotcueNumber
    property color fallbackColor: Theme.red
    readonly property color hotcueColor: {
        const value = colorProxy.value;
        if (!Number.isFinite(value) || value === -1)
            return root.fallbackColor;

        const rgb = (Math.round(value) >>> 0) & 0xFFFFFF;
        return "#" + rgb.toString(16).padStart(6, "0");
    }
    readonly property bool isSet: statusProxy.value > 0

    signal popupRequested(real mouseX, real mouseY)
    signal cleared()

    key: "hotcue_" + root.hotcueNumber + "_activate"
    displayKey: "hotcue_" + root.hotcueNumber + "_status"

    Mixxx.ControlProxy {
        id: colorProxy

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_color"
    }

    Mixxx.ControlProxy {
        id: statusProxy

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_status"

        onValueChanged: function(newValue) {
            if (newValue === 0) {
                root.cleared();
            }
        }
    }

    onSecondaryPressed: function(displayValue, mouseX, mouseY) {
        if (displayValue > 0) {
            root.popupRequested(mouseX, mouseY);
        }
    }
}
