import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick

// The effect chooser was the last dropdown still wearing Qt Quick's default
// chrome, so it sat in the effects rack looking like a control borrowed from
// another program. Skin.ComboBox is the dropdown every other one here uses.
Skin.ComboBox {
    id: root

    // Below this width the field drops its arrows: on a small screen they took
    // a third of it, and the name read as "Fil..." or "D...".
    readonly property int arrowsFromWidth: 110
    // The list is at least this wide whatever the field's width, so that names
    // can be read in full where they are chosen.
    readonly property int minimumListWidth: 190
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

    indicator.visible: root.width >= root.arrowsFromWidth
    indicator.width: root.width >= root.arrowsFromWidth ? 20 : 0
    model: Mixxx.EffectsManager.visibleEffectsModel
    popupWidth: Math.max(root.width, root.minimumListWidth)
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
