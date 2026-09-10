pragma ComponentBehavior: Bound
import "Effects" as Effects
import QtQuick 2.12

// One effect slot of an effect unit, drawn at touch size: the effect chooser,
// its ON button, its meta knob, and a knob or button per parameter of whatever
// effect is currently loaded.
//
// This is the one part of the surface that cannot be described by a rect and a
// group/key, because the controls it needs are not known until an effect is
// loaded -- Echo has three parameters, Filter has one, and a layout file cannot
// name them in advance. It reads the same EffectsManager slot the main window
// does, so the parameter set follows the loaded effect and the two surfaces
// always agree.
//
// Spec: {"type": "effect", "unit": 1, "effect": 1, "rect": [...]}
//   unit    1..4, the effect unit (default 1)
//   effect  1..3, the slot within it (default 1)
//   cellWidth / knobSize   per-parameter sizing in canvas units
//   linkControls           true brings back the desktop LINK/invert row
Item {
    id: root

    readonly property int effectNumber: root.spec.effect ?? 1
    required property var spec
    property var surface: null
    // Element-local pixels per canvas unit, so sizes in the layout JSON mean the
    // same thing here as they do in a rect.
    readonly property real uiScale: {
        const box = root.surface ? root.surface.elementRect(root.spec) : root.spec.rect;
        return (box && box[2] > 0) ? root.width / box[2] : 1;
    }
    readonly property int unitNumber: root.spec.unit ?? 1

    Effects.EffectSlot {
        anchors.fill: parent
        cellWidth: (root.spec.cellWidth ?? 110) * root.uiScale
        effectNumber: root.effectNumber
        // The parameters are the whole point of putting this on the surface;
        // collapsed it would be a chooser and nothing else.
        expanded: true
        focusButtonSize: 0
        knobSize: (root.spec.knobSize ?? 64) * root.uiScale
        maxSelectorWidth: (root.spec.selectorWidth ?? 420) * root.uiScale
        parameterButtonHeight: (root.spec.buttonHeight ?? 48) * root.uiScale
        selectorControlWidth: (root.spec.controlWidth ?? 80) * root.uiScale
        showParameterLinkControls: root.spec.linkControls === true
        unitNumber: root.unitNumber
    }
    // Inset frame, matching the other surface elements.
    Rectangle {
        anchors.fill: parent
        border.color: "#000000"
        border.width: 1
        color: "transparent"
        opacity: 0.5
    }
}
