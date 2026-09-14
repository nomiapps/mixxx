import "." as Skin
import "AccessibleNames.js" as AccessibleNames
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Skin.MiniKnob {
    id: root

    property alias group: control.group
    property alias key: control.key

    Accessible.name: AccessibleNames.forControl(root.group, root.key)
    value: control.parameter

    onTurned: turnedValue => control.parameter = turnedValue
    // An assistive technology sets a value by writing `value` itself: UI Automation's
    // RangeValue.SetValue lands in QAccessibleQuickItem::setCurrentValue, which is a
    // plain property write. That drew the new position but never reached the control,
    // and it replaced the binding above, so the widget stopped following the control
    // for good. Forward such a write to the control and re-attach the binding. Not
    // while pressed: a drag holds `value` through its own Binding and moves the
    // control through the signal, and the two are equal there anyway.
    onValueChanged: {
        if (root.pressed || Math.abs(root.value - control.parameter) < 1e-9)
            return;
        if (control.initialized)
            control.parameter = root.value;
        root.value = Qt.binding(() => control.parameter);
    }

    Mixxx.ControlProxy {
        id: control
    }
    TapHandler {
        onDoubleTapped: control.reset()
    }
}
