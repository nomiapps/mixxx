import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

// What a controller's BEATS encoder is set to: the value of the one knob it
// turns on the effect that is picked, with that knob's name under it. The
// encoder has no display of its own, so without this a turn of it can only be
// judged by ear.
Item {
    id: root

    // The parameters that are counted in beats. Mixxx tells the skin a
    // parameter's unit only where it has a symbol, and beats have none.
    readonly property var beatParameterIds: ["delay_time", "speed", "lfo_period"]
    // The knob's control in the slot: "parameter1" and so on.
    property string controlKey: ""
    readonly property var parameter: {
        // Read so that this is worked out again when another effect is loaded.
        const effectId = root.slot ? root.slot.effectId : "";
        if (!root.slot || !effectId || !root.controlKey)
            return null;
        const model = root.slot.parametersModel;
        for (let row = 0; row < model.rowCount(); ++row) {
            const item = model.get(row);
            if (item.controlKey === root.controlKey && item.type === Mixxx.EffectSlotParametersModel.Knob)
                return item.loaded ? item : null;
        }
        return null;
    }
    // Null while no effect is picked.
    property Mixxx.EffectSlotProxy slot: null
    readonly property string valueText: {
        if (!root.parameter || !valueLoader.item)
            return "—";
        const value = valueLoader.item.value;
        if (root.parameter.unitString === "Hz")
            return value >= 1000 ? (value / 1000).toFixed(1) + " kHz" : Math.round(value) + " Hz";
        if (root.beatParameterIds.indexOf(root.parameter.parameterId) !== -1)
            return root.beats(value);
        return (Math.abs(value) >= 100 ? value.toFixed(0) : value.toFixed(2)) + (root.parameter.unitString ? " " + root.parameter.unitString : "");
    }

    // 0.5 as "1/2", 1.5 as "1 1/2", 8 as "8": the way beat lengths are said.
    // Anything that is not a whole number of eighths is shown as it is.
    function beats(value) {
        const eighths = Math.round(value * 8);
        if (Math.abs(value * 8 - eighths) > 0.02 || eighths <= 0)
            return value.toFixed(2);
        const whole = Math.floor(eighths / 8);
        let numerator = eighths % 8;
        let denominator = 8;
        while (numerator > 0 && numerator % 2 === 0) {
            numerator /= 2;
            denominator /= 2;
        }
        const fraction = numerator > 0 ? numerator + "/" + denominator : "";
        return [whole > 0 ? String(whole) : "", fraction].filter(part => part).join(" ");
    }

    Text {
        id: valueLabel

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 6
        color: root.parameter ? Theme.effectColor : Theme.buttonNormalColor
        font.bold: true
        font.family: Theme.fontFamily
        font.pixelSize: 18
        text: root.valueText
    }
    Skin.EmbeddedText {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        elide: Text.ElideRight
        font.bold: false
        horizontalAlignment: Text.AlignHCenter
        text: root.parameter ? "BEATS · " + (root.parameter.shortName || root.parameter.name) : "BEATS"
        width: parent.width
    }
    // Only while there is a knob to read: a control proxy with nothing to
    // point at logs a missing control.
    Loader {
        id: valueLoader

        active: root.parameter !== null

        sourceComponent: Mixxx.ControlProxy {
            group: root.slot.group
            key: root.controlKey
        }
    }
}
