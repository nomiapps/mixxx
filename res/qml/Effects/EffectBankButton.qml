import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

// One button of the effect bank: the effect in one slot, lit while it is on.
// It says what the slot does, so the two Filter slots a controller prints as
// HPF and LPF read as that here too.
Skin.Button {
    id: root

    required property int effectNumber
    readonly property string filterEffectId: "org.mixxx.effects.filter"
    // A filter cutoff counts as in use once it is off the end of its range,
    // where the filter does nothing: 13 Hz for the high-pass, 22050 Hz for the
    // low-pass.
    readonly property bool highPassInUse: highPassCutoff.value > 20
    readonly property string label: {
        if (!root.slot.loaded)
            return "—";
        if (root.slot.effectId === root.filterEffectId && root.highPassInUse !== root.lowPassInUse)
            return root.highPassInUse ? "HPF" : "LPF";
        return root.slot.effectName;
    }
    readonly property bool lowPassInUse: lowPassCutoff.value < 20000
    readonly property bool picked: enabledControl.value > 0
    readonly property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, root.effectNumber)
    required property int unitNumber

    function setPicked(on) {
        enabledControl.value = on ? 1 : 0;
    }

    accessibleName: root.slot.loaded ? root.slot.effectName : "Empty effect slot"
    activeColor: Theme.effectColor
    enabled: root.slot.loaded
    highlight: root.picked
    text: root.label

    Mixxx.ControlProxy {
        id: enabledControl

        group: root.slot.group
        key: "enabled"
    }
    // The Filter effect's knobs: low-pass cutoff first, high-pass cutoff third.
    Mixxx.ControlProxy {
        id: lowPassCutoff

        group: root.slot.group
        key: "parameter1"
    }
    Mixxx.ControlProxy {
        id: highPassCutoff

        group: root.slot.group
        key: "parameter3"
    }
}
