import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

// A generic performance pad: pressing writes 1 to (padGroup, padKey), releasing
// writes 0 (Mixxx triggers fire on the rising edge; momentary controls get a
// proper press/release). Lights up from (activeGroup, activeKey).
//
// Set stopKey to make the pad a toggle: while it is lit, a press goes to that
// control instead, so a one-shot pad can also stop what it started. It defaults
// to padKey, which is the plain pad -- both halves of the choice write the same
// control, and a proxy is never left holding an empty key (QmlControlProxy warns
// and unbinds itself on one).
//
// The pad remembers which proxy it pressed rather than deciding again on
// release: the light goes out under the finger, and releasing the other control
// would leave the pressed one latched at 1, deaf to every later press.
Skin.Button {
    id: root

    property string activeGroup: padGroup
    property string activeKey: padKey
    property var heldControl: null
    required property string padGroup
    required property string padKey
    property string stopKey: padKey

    function release() {
        if (!root.heldControl)
            return;
        root.heldControl.value = 0;
        root.heldControl = null;
    }

    highlight: activeControl.value != 0

    onCanceled: root.release()
    onPressed: {
        root.heldControl = root.highlight ? stopControl : control;
        root.heldControl.value = 1;
    }
    onReleased: root.release()

    Mixxx.ControlProxy {
        id: control

        group: root.padGroup
        key: root.padKey
    }
    Mixxx.ControlProxy {
        id: stopControl

        group: root.padGroup
        key: root.stopKey
    }
    Mixxx.ControlProxy {
        id: activeControl

        group: root.activeGroup
        key: root.activeKey
    }
}
