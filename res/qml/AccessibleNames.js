.pragma library

// Names for screen readers, derived from the control a widget is bound to.
//
// Qt gives a QML item an accessible role if its type declares one -- AbstractButton
// and the QtQuick.Controls types do -- but never a name: for a button it uses `text`,
// so every icon-only button in the skin arrives at the screen reader nameless. A UI
// Automation walk of the New UI found 23 of 54 exposed buttons with an empty name,
// and no fader or knob exposing anything at all.
//
// Almost every one of those is bound to a Mixxx control, and the (group, key) pair
// already says what it does. This turns that pair into something speakable, so a
// control gets a usable name for free at the point it is bound rather than needing
// one written out at each of the hundreds of call sites. A call site with a better
// name than the key can still set Accessible.name itself and win.

// Keys whose name is not obvious from the key itself. Anything absent falls through
// to the underscores-to-spaces rule below, which is right more often than not
// ("beatloop_size" -> "beatloop size").
var _keyNames = {
    "crossfader": "Crossfader",
    "cue_default": "Cue",
    "cue_gotoandplay": "Play from cue",
    "cue_gotoandstop": "Stop at cue",
    "headGain": "Headphone gain",
    "headMix": "Headphone mix",
    "headSplit": "Split cue",
    "keylock": "Key lock",
    "main_mix": "Main mix",
    "orientation": "Crossfader side",
    "pfl": "Headphone cue",
    "pregain": "Gain",
    "quantize": "Quantize",
    "rate": "Tempo",
    "rate_perm_down": "Tempo down",
    "rate_perm_up": "Tempo up",
    "rate_temp_down": "Tempo nudge down",
    "rate_temp_up": "Tempo nudge up",
    "reverse": "Reverse",
    "slip_enabled": "Slip mode",
    "sync_enabled": "Sync",
    "volume": "Volume"
};

// "[Channel2]" -> "Deck 2", "[EqualizerRack1_[Channel1]_Effect1]" -> "Deck 1 equalizer".
// Groups that do not match a known shape come back with their brackets and
// underscores removed, which is still better than nothing being spoken.
function forGroup(group) {
    if (!group)
        return "";
    var inner = group.replace(/^\[/, "").replace(/\]$/, "");
    // Rack groups embed the channel they act on: pull that out and label the rack.
    var nested = inner.match(/^([A-Za-z]+)Rack\d+_\[([A-Za-z]+)(\d*)\]/);
    if (nested)
        return forGroup("[" + nested[2] + nested[3] + "]") + " " + _spaced(nested[1]).toLowerCase();
    var m = inner.match(/^([A-Za-z]+?)(\d+)$/);
    var name = m ? m[1] : inner;
    var number = m ? " " + m[2] : "";
    switch (name) {
    case "Channel":
        return "Deck" + number;
    case "PreviewDeck":
        return "Preview deck" + number;
    case "Headphone":
        return "Headphones";
    case "Master":
        return "Main";
    case "Microphone":
        return "Mic" + number;
    default:
        return name.replace(/_/g, " ") + number;
    }
}

// "QuickEffect" -> "Quick Effect", so a run-together group name is read as words.
function _spaced(name) {
    return name.replace(/([a-z0-9])([A-Z])/g, "$1 $2");
}

function forKey(key) {
    if (!key)
        return "";
    if (_keyNames.hasOwnProperty(key))
        return _keyNames[key];
    // Unmapped keys are read as they are written, minus the underscores, with a
    // capital so the name does not start mid-word: "beatloop_size" -> "Beatloop size".
    var words = key.replace(/_/g, " ");
    return words.charAt(0).toUpperCase() + words.slice(1);
}

// The name a screen reader reads out: what it does, then what it acts on --
// "Volume, deck 2" rather than "[Channel2],volume".
function forControl(group, key) {
    var k = forKey(key);
    var g = forGroup(group);
    if (!k)
        return g;
    if (!g)
        return k;
    return k + ", " + g.toLowerCase();
}
