import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick

// The effect chooser was the last dropdown still wearing Qt Quick's default
// chrome, so it sat in the effects rack looking like a control borrowed from
// another program. Skin.ComboBox is the dropdown every other one here uses.
Skin.ComboBox {
    id: root

    required property Mixxx.EffectSlotProxy slot

    function syncCurrentEffect() {
        for (let index = 0; index < count; ++index) {
            if (model.get(index).effectId === slot.effectId) {
                currentIndex = index;
                return;
            }
        }
        currentIndex = 0;
    }

    model: Mixxx.EffectsManager.visibleEffectsModel
    textRole: "display"

    Component.onCompleted: syncCurrentEffect()
    onActivated: index => {
        const effectId = model.get(index).effectId || "";
        if (slot.effectId !== effectId) {
            slot.effectId = effectId;
        }
    }
    onCountChanged: syncCurrentEffect()

    Connections {
        function onEffectIdChanged() {
            root.syncCurrentEffect();
        }

        target: root.slot
    }
}
