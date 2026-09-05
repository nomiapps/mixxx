// Generic MIDI keyboard -> the built-in synthesizer channel.
//
// note_on takes the MIDI note plus velocity / 128 in the fraction, so a bare
// integer is full velocity. Held-note state lives in the engine; this script
// only adds the sustain pedal, which keeps releases back until it is lifted.

var SynthKeyboard = {};

SynthKeyboard.group = "[Synth1]";
SynthKeyboard.sustain = false;
// notes released while the pedal was down, to let go when it comes up
SynthKeyboard.pending = {};

SynthKeyboard.init = function() {
    SynthKeyboard.sustain = false;
    SynthKeyboard.pending = {};
};

SynthKeyboard.shutdown = function() {
    engine.setValue(SynthKeyboard.group, "all_notes_off", 1);
    engine.setValue(SynthKeyboard.group, "all_notes_off", 0);
};

SynthKeyboard.noteOn = function(channel, control, value, status, group) {
    if (value === 0) {
        // note-on with velocity 0 is a note-off under running status
        SynthKeyboard.noteOff(channel, control, value, status, group);
        return;
    }
    delete SynthKeyboard.pending[control];
    engine.setValue(SynthKeyboard.group, "note_on", control + value / 128);
};

SynthKeyboard.noteOff = function(channel, control, value, status, group) {
    if (SynthKeyboard.sustain) {
        SynthKeyboard.pending[control] = true;
        return;
    }
    engine.setValue(SynthKeyboard.group, "note_off", control);
};

SynthKeyboard.sustainPedal = function(channel, control, value, status, group) {
    var down = value >= 64;
    if (down === SynthKeyboard.sustain) {
        return;
    }
    SynthKeyboard.sustain = down;
    if (!down) {
        for (var note in SynthKeyboard.pending) {
            engine.setValue(SynthKeyboard.group, "note_off", Number(note));
        }
        SynthKeyboard.pending = {};
    }
};

SynthKeyboard.modWheel = function(channel, control, value, status, group) {
    engine.setParameter(SynthKeyboard.group, "cutoff", value / 127);
};

SynthKeyboard.allNotesOff = function(channel, control, value, status, group) {
    SynthKeyboard.pending = {};
    engine.setValue(SynthKeyboard.group, "all_notes_off", 1);
    engine.setValue(SynthKeyboard.group, "all_notes_off", 0);
};
