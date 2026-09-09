// Novation Launchpad X mapping for Mixxx.
//
// This is an INSTRUMENT mapping, not a deck mapping: the 8x8 grid plays the
// built-in synth ([Synth1]) and edits the step sequencer ([Sequencer1]). To
// mix from a Launchpad, use the Launchpad Mini MK3 / MK2 mappings instead.
//
// The pad is driven in Programmer mode, where the grid reports note = row * 10
// + column (row 1 at the bottom, column 1 at the left), the top row reports CC
// 91..98 and the right-hand column CC 89, 79, ... 19 from the top down. Pads
// are velocity sensitive, and [Synth1] takes velocity as a fraction of the
// note number, so the reason for playing this rather than clicking a keyboard
// survives the trip.

var LaunchpadX = {};

// ---------------------------------------------------------------- constants

LaunchpadX.synthGroup = "[Synth1]";
LaunchpadX.seqGroup = "[Sequencer1]";
LaunchpadX.steps = 16;
LaunchpadX.samplerLanes = 4;

// F0 00 20 29 02 0C ... : Novation, Launchpad X (0x0C). The Mini MK3 mapping
// in this tree sends the same messages with 0x0D.
LaunchpadX.sysexHeader = [0xF0, 0x00, 0x20, 0x29, 0x02, 0x0C];
LaunchpadX.modeLive = 0;
LaunchpadX.modeProgrammer = 1;

LaunchpadX.MODE_NOTES = 0;
LaunchpadX.MODE_STEPS = 1;

// Top row, left to right.
LaunchpadX.CC_NOTES = 0x5B;      // 91
LaunchpadX.CC_STEPS = 0x5C;      // 92
LaunchpadX.CC_OCT_DOWN = 0x5D;   // 93
LaunchpadX.CC_OCT_UP = 0x5E;     // 94
LaunchpadX.CC_SCALE = 0x5F;      // 95
LaunchpadX.CC_RECORD = 0x60;     // 96
LaunchpadX.CC_RUN = 0x61;        // 97
LaunchpadX.CC_CLEAR = 0x62;      // 98

// Right-hand column, top to bottom.
LaunchpadX.rightColumn = [89, 79, 69, 59, 49, 39, 29, 19];

// Colours are Novation RGB, 0..127 per channel.
LaunchpadX.colors = {
    off: [0, 0, 0],
    root: [0, 24, 127],
    inScale: [16, 16, 24],
    outOfScale: [3, 3, 3],
    held: [0, 127, 48],
    stepSynth: [127, 52, 0],
    stepSampler: [72, 0, 110],
    stepEmpty: [5, 5, 7],
    playhead: [127, 127, 127],
    laneDark: [0, 0, 0],
    modeOn: [0, 96, 127],
    modeOff: [4, 12, 16],
    recordOn: [127, 0, 0],
    recordOff: [16, 2, 2],
    runOn: [0, 127, 32],
    runOff: [2, 16, 6],
    action: [24, 24, 8],
    sampler: [96, 40, 0]
};

// Every note set: the value scale_mask carries when no scale is chosen.
LaunchpadX.chromaticMask = 4095;

// Scales as semitone offsets from the root. Only the NAMES need this table --
// the mask on the control says which notes are in the scale, so the pad and
// the screen cannot disagree even if one of them offers scales the other
// does not.
LaunchpadX.scales = [
    {name: "Chromatic", degrees: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]},
    {name: "Major", degrees: [0, 2, 4, 5, 7, 9, 11]},
    {name: "Minor", degrees: [0, 2, 3, 5, 7, 8, 10]},
    {name: "Dorian", degrees: [0, 2, 3, 5, 7, 9, 10]},
    {name: "Pentatonic minor", degrees: [0, 3, 5, 7, 10]},
    {name: "Blues", degrees: [0, 3, 5, 6, 7, 10]}
];

// ------------------------------------------------------------------- state

// The key, the scale and the octave are NOT kept here. They live in the
// synth's scale_mask / scale_root / base_note controls, so the keyboard on
// screen and this pad are the same instrument in the same key: transpose
// either and the other follows.
LaunchpadX.mode = LaunchpadX.MODE_NOTES;
LaunchpadX.recording = false;
// The scale button doubles as a modifier. Held, a pad sets the root; tapped
// on its own, it steps to the next scale.
LaunchpadX.scaleHeld = false;
LaunchpadX.scalePickedRoot = false;
LaunchpadX.stepPage = 0;    // 0 = steps 1..8, 1 = steps 9..16
// MIDI note -> how many pads are holding it, so a note that appears twice in
// the overlapping rows releases only when the last finger lifts.
LaunchpadX.heldNotes = {};
LaunchpadX.connections = [];
LaunchpadX.lastPlayhead = -1;

// -------------------------------------------------------------- pad helpers

/**
 * The programmer-mode index of a grid pad.
 *
 * @param {number} row 0 at the bottom, 7 at the top
 * @param {number} col 0 at the left, 7 at the right
 * @returns {number} the note number the pad sends and lights on
 */
LaunchpadX.padIndex = function(row, col) {
    return (row + 1) * 10 + (col + 1);
};

/**
 * Split a programmer-mode pad index back into grid coordinates.
 *
 * @param {number} index the note number a pad sent
 * @returns {object} {row, col}, or null when it is not a grid pad
 */
LaunchpadX.padCoords = function(index) {
    const row = Math.floor(index / 10) - 1;
    const col = (index % 10) - 1;
    if (row < 0 || row > 7 || col < 0 || col > 7) {
        return null;
    }
    return {row: row, col: col};
};

/**
 * Light one pad or button.
 *
 * @param {number} index programmer-mode index of the pad or button
 * @param {Array} color [r, g, b], each 0..127
 */
LaunchpadX.light = function(index, color) {
    const msg = LaunchpadX.sysexHeader.concat(
        [0x03, 0x03, index, color[0], color[1], color[2], 0xF7]);
    midi.sendSysexMsg(msg, msg.length);
};

// ------------------------------------------------------------- note layout

/**
 * Semitone offsets to the 12-bit mask scale_mask carries.
 *
 * @param {Array} degrees semitone offsets from the root
 * @returns {number} bit N set when N semitones above the root is in the scale
 */
LaunchpadX.maskFor = function(degrees) {
    let mask = 0;
    for (let i = 0; i < degrees.length; i++) {
        mask |= 1 << degrees[i];
    }
    return mask;
};

/**
 * The lowest note on the grid.
 *
 * @returns {number} a MIDI note number
 */
LaunchpadX.baseNote = function() {
    return Math.round(engine.getValue(LaunchpadX.synthGroup, "base_note"));
};

/**
 * The pitch class the scale is rooted on.
 *
 * @returns {number} 0..11, 0 = C
 */
LaunchpadX.scaleRoot = function() {
    const root = Math.round(engine.getValue(LaunchpadX.synthGroup, "scale_root"));
    return ((root % 12) + 12) % 12;
};

/**
 * The scale as twelve bits. An empty or missing mask reads as chromatic, so a
 * control that never got a value cannot silence the grid.
 *
 * @returns {number} the mask, always with at least the root bit set
 */
LaunchpadX.scaleMask = function() {
    const mask = Math.round(engine.getValue(LaunchpadX.synthGroup, "scale_mask")) & 0xFFF;
    return mask > 0 ? mask : LaunchpadX.chromaticMask;
};

/**
 * The scale as ascending semitone offsets.
 *
 * @returns {Array} the offsets from the root that are in the scale
 */
LaunchpadX.scaleDegrees = function() {
    const mask = LaunchpadX.scaleMask();
    const degrees = [];
    for (let i = 0; i < 12; i++) {
        if (mask & (1 << i)) {
            degrees.push(i);
        }
    }
    return degrees;
};

/**
 * Whether no scale is selected, in which case the grid is laid out
 * chromatically rather than dropping notes nothing has excluded.
 *
 * @returns {boolean} true when every note is in the scale
 */
LaunchpadX.isChromatic = function() {
    return LaunchpadX.scaleMask() === LaunchpadX.chromaticMask;
};

/**
 * The MIDI note a pad plays under the current layout.
 *
 * @param {number} row 0 at the bottom
 * @param {number} col 0 at the left
 * @returns {number} a MIDI note number, or -1 where the layout runs off the end
 */
LaunchpadX.noteFor = function(row, col) {
    let note;
    const base = LaunchpadX.baseNote();
    if (LaunchpadX.isChromatic()) {
        // The chromatic layout: a row spans 8 semitones and the next row starts
        // a fourth up, so a shape played anywhere transposes anywhere.
        note = base + row * 5 + col;
    } else {
        const degrees = LaunchpadX.scaleDegrees();
        // Three scale degrees per row is roughly a fourth in a 7-note scale,
        // which keeps the fingering the chromatic layout teaches.
        const degree = row * 3 + col;
        const octave = Math.floor(degree / degrees.length);
        const within = degree - octave * degrees.length;
        note = base + LaunchpadX.scaleRoot() + octave * 12 + degrees[within];
    }
    if (note < 0 || note > 127) {
        return -1;
    }
    return note;
};

/**
 * Whether a note is a degree of the selected scale.
 *
 * @param {number} note a MIDI note number
 * @returns {boolean} true when the note is in the scale
 */
LaunchpadX.isInScale = function(note) {
    const pitchClass = ((note - LaunchpadX.scaleRoot()) % 12 + 12) % 12;
    return (LaunchpadX.scaleMask() & (1 << pitchClass)) !== 0;
};

/**
 * Whether a note is the root of the selected scale.
 *
 * @param {number} note a MIDI note number
 * @returns {boolean} true when the note is a root
 */
LaunchpadX.isRoot = function(note) {
    return ((note - LaunchpadX.scaleRoot()) % 12 + 12) % 12 === 0;
};

/**
 * The resting colour of a pad in notes mode.
 *
 * @param {number} note the note the pad plays, or -1
 * @returns {Array} an [r, g, b] colour
 */
LaunchpadX.noteColor = function(note) {
    if (note < 0) {
        return LaunchpadX.colors.off;
    }
    if (LaunchpadX.heldNotes[note]) {
        return LaunchpadX.colors.held;
    }
    if (LaunchpadX.isRoot(note)) {
        return LaunchpadX.colors.root;
    }
    if (LaunchpadX.isInScale(note)) {
        return LaunchpadX.colors.inScale;
    }
    return LaunchpadX.colors.outOfScale;
};

// --------------------------------------------------------------- sequencer

/**
 * The control key for one field of one synth step.
 *
 * @param {number} step 0-based step index
 * @param {string} field enabled, note, velocity or gate
 * @returns {string} the control key
 */
LaunchpadX.synthStepKey = function(step, field) {
    return "synth_step_" + (step + 1) + "_" + field;
};

/**
 * The control key for one step of one sampler lane.
 *
 * @param {number} lane 0-based lane index
 * @param {number} step 0-based step index
 * @returns {string} the control key
 */
LaunchpadX.samplerStepKey = function(lane, step) {
    return "sampler_" + (lane + 1) + "_step_" + (step + 1) + "_enabled";
};

/**
 * Write a played note into the pattern, quantised to the sounding step.
 *
 * current_step is the step the sequencer is playing right now, so a note
 * played a little late - which is how people play - lands on the step it was
 * aimed at rather than the one after it.
 *
 * @param {number} note the MIDI note that was played
 * @param {number} velocity MIDI velocity, 1..127
 */
LaunchpadX.captureNote = function(note, velocity) {
    let step = engine.getValue(LaunchpadX.seqGroup, "current_step");
    if (step === undefined || step < 0) {
        step = 0;
    }
    step = Math.floor(step) % LaunchpadX.steps;
    engine.setValue(LaunchpadX.seqGroup, LaunchpadX.synthStepKey(step, "note"), note);
    engine.setValue(LaunchpadX.seqGroup, LaunchpadX.synthStepKey(step, "velocity"), velocity / 127);
    engine.setValue(LaunchpadX.seqGroup, LaunchpadX.synthStepKey(step, "enabled"), 1);
};

/**
 * Clear every lane of the pattern.
 */
LaunchpadX.clearPattern = function() {
    for (let step = 0; step < LaunchpadX.steps; step++) {
        engine.setValue(LaunchpadX.seqGroup, LaunchpadX.synthStepKey(step, "enabled"), 0);
        for (let lane = 0; lane < LaunchpadX.samplerLanes; lane++) {
            engine.setValue(LaunchpadX.seqGroup, LaunchpadX.samplerStepKey(lane, step), 0);
        }
    }
    LaunchpadX.draw();
};

// ----------------------------------------------------------------- drawing

/**
 * The colour of one pad in steps mode.
 *
 * Rows run top down: the synth lane, then the four sampler lanes. The bottom
 * three rows stay dark; they are where per-step velocity would go.
 *
 * @param {number} row 0 at the bottom
 * @param {number} col 0 at the left
 * @returns {Array} an [r, g, b] colour
 */
LaunchpadX.stepColor = function(row, col) {
    const lane = 7 - row;
    const step = LaunchpadX.stepPage * 8 + col;
    if (lane > LaunchpadX.samplerLanes) {
        return LaunchpadX.colors.laneDark;
    }
    const playing = Math.floor(engine.getValue(LaunchpadX.seqGroup, "current_step"));
    const key = lane === 0
        ? LaunchpadX.synthStepKey(step, "enabled")
        : LaunchpadX.samplerStepKey(lane - 1, step);
    const on = engine.getValue(LaunchpadX.seqGroup, key) > 0;
    if (step === playing) {
        return LaunchpadX.colors.playhead;
    }
    if (!on) {
        return LaunchpadX.colors.stepEmpty;
    }
    return lane === 0 ? LaunchpadX.colors.stepSynth : LaunchpadX.colors.stepSampler;
};

/**
 * Repaint the 8x8 grid for the current mode.
 */
LaunchpadX.drawGrid = function() {
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const index = LaunchpadX.padIndex(row, col);
            if (LaunchpadX.mode === LaunchpadX.MODE_NOTES) {
                LaunchpadX.light(index, LaunchpadX.noteColor(LaunchpadX.noteFor(row, col)));
            } else {
                LaunchpadX.light(index, LaunchpadX.stepColor(row, col));
            }
        }
    }
};

/**
 * Repaint the top row and the right-hand column.
 */
LaunchpadX.drawButtons = function() {
    const c = LaunchpadX.colors;
    LaunchpadX.light(LaunchpadX.CC_NOTES,
        LaunchpadX.mode === LaunchpadX.MODE_NOTES ? c.modeOn : c.modeOff);
    LaunchpadX.light(LaunchpadX.CC_STEPS,
        LaunchpadX.mode === LaunchpadX.MODE_STEPS ? c.modeOn : c.modeOff);
    LaunchpadX.light(LaunchpadX.CC_OCT_DOWN, c.action);
    LaunchpadX.light(LaunchpadX.CC_OCT_UP, c.action);
    LaunchpadX.light(LaunchpadX.CC_SCALE, c.action);
    LaunchpadX.light(LaunchpadX.CC_RECORD, LaunchpadX.recording ? c.recordOn : c.recordOff);
    LaunchpadX.light(LaunchpadX.CC_RUN,
        engine.getValue(LaunchpadX.seqGroup, "run") > 0 ? c.runOn : c.runOff);
    LaunchpadX.light(LaunchpadX.CC_CLEAR, c.action);

    for (let i = 0; i < LaunchpadX.rightColumn.length; i++) {
        let color;
        if (LaunchpadX.mode === LaunchpadX.MODE_NOTES) {
            color = c.sampler;
        } else {
            color = i === 0 ? c.modeOff : c.off;
        }
        LaunchpadX.light(LaunchpadX.rightColumn[i], color);
    }
};

/**
 * Repaint everything.
 */
LaunchpadX.draw = function() {
    LaunchpadX.drawGrid();
    LaunchpadX.drawButtons();
};

/**
 * Move the playhead in steps mode without repainting the whole grid.
 *
 * @param {number} value the new current_step
 */
LaunchpadX.onStepChanged = function(value) {
    const previous = LaunchpadX.lastPlayhead;
    LaunchpadX.lastPlayhead = Math.floor(value);
    if (LaunchpadX.mode !== LaunchpadX.MODE_STEPS) {
        return;
    }
    const columns = [previous, LaunchpadX.lastPlayhead];
    for (let i = 0; i < columns.length; i++) {
        const col = columns[i] - LaunchpadX.stepPage * 8;
        if (columns[i] < 0 || col < 0 || col > 7) {
            continue;
        }
        for (let lane = 0; lane <= LaunchpadX.samplerLanes; lane++) {
            const row = 7 - lane;
            LaunchpadX.light(LaunchpadX.padIndex(row, col), LaunchpadX.stepColor(row, col));
        }
    }
};

// ------------------------------------------------------------------- input

/**
 * Start a note, counting the pads holding it.
 *
 * @param {number} note a MIDI note number
 * @param {number} velocity MIDI velocity, 1..127
 */
LaunchpadX.noteOn = function(note, velocity) {
    const held = LaunchpadX.heldNotes[note] || 0;
    LaunchpadX.heldNotes[note] = held + 1;
    if (held === 0) {
        // [Synth1] reads velocity out of the fractional part: note + v / 128.
        engine.setValue(LaunchpadX.synthGroup, "note_on", note + velocity / 128);
    }
    if (LaunchpadX.recording) {
        LaunchpadX.captureNote(note, velocity);
    }
};

/**
 * Release a note once the last pad holding it is lifted.
 *
 * @param {number} note a MIDI note number
 */
LaunchpadX.noteOff = function(note) {
    const held = LaunchpadX.heldNotes[note] || 0;
    if (held <= 1) {
        delete LaunchpadX.heldNotes[note];
        engine.setValue(LaunchpadX.synthGroup, "note_off", note);
    } else {
        LaunchpadX.heldNotes[note] = held - 1;
    }
};

/**
 * Repaint every pad that plays a given note, in either overlapping row.
 *
 * @param {number} note a MIDI note number
 */
LaunchpadX.refreshNote = function(note) {
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            if (LaunchpadX.noteFor(row, col) === note) {
                LaunchpadX.light(LaunchpadX.padIndex(row, col), LaunchpadX.noteColor(note));
            }
        }
    }
};

/**
 * Toggle one step of one lane.
 *
 * @param {number} row 0 at the bottom
 * @param {number} col 0 at the left
 */
LaunchpadX.toggleStep = function(row, col) {
    const lane = 7 - row;
    if (lane > LaunchpadX.samplerLanes) {
        return;
    }
    const step = LaunchpadX.stepPage * 8 + col;
    const key = lane === 0
        ? LaunchpadX.synthStepKey(step, "enabled")
        : LaunchpadX.samplerStepKey(lane - 1, step);
    engine.setValue(LaunchpadX.seqGroup, key,
        engine.getValue(LaunchpadX.seqGroup, key) > 0 ? 0 : 1);
    LaunchpadX.light(LaunchpadX.padIndex(row, col), LaunchpadX.stepColor(row, col));
};

/**
 * A grid pad was pressed or released.
 *
 * @param {number} channel MIDI channel
 * @param {number} control the programmer-mode index of the pad
 * @param {number} value velocity; 0 means released
 * @param {number} status the MIDI status byte
 */
LaunchpadX.onPad = function(channel, control, value, status) {
    const coords = LaunchpadX.padCoords(control);
    if (coords === null) {
        return;
    }
    // In programmer mode a release is a note-on with velocity 0, but honour a
    // real note-off too in case the pad is configured otherwise.
    const pressed = (status & 0xF0) === 0x90 && value > 0;
    if (LaunchpadX.mode === LaunchpadX.MODE_STEPS) {
        if (pressed) {
            LaunchpadX.toggleStep(coords.row, coords.col);
        }
        return;
    }
    const note = LaunchpadX.noteFor(coords.row, coords.col);
    if (note < 0) {
        return;
    }
    // Held scale button: the pad picks the key rather than playing it.
    if (LaunchpadX.scaleHeld) {
        if (pressed) {
            LaunchpadX.scalePickedRoot = true;
            LaunchpadX.setRootFromNote(note);
        }
        return;
    }
    if (pressed) {
        LaunchpadX.noteOn(note, value);
    } else {
        LaunchpadX.noteOff(note);
    }
    LaunchpadX.refreshNote(note);
};

/**
 * The right-hand column: sampler triggers, or the pattern page in steps mode.
 *
 * @param {number} index 0 at the top
 * @param {boolean} pressed true on press
 */
LaunchpadX.onRightColumn = function(index, pressed) {
    if (!pressed) {
        return;
    }
    if (LaunchpadX.mode === LaunchpadX.MODE_STEPS) {
        if (index === 0) {
            LaunchpadX.stepPage = LaunchpadX.stepPage === 0 ? 1 : 0;
            LaunchpadX.draw();
        }
        return;
    }
    engine.setValue("[Sampler" + (index + 1) + "]", "cue_gotoandplay", 1);
};

/**
 * Move the keyboard by an octave.
 *
 * @param {number} direction -1 or 1
 */
LaunchpadX.shiftOctave = function(direction) {
    const next = LaunchpadX.baseNote() + direction * 12;
    if (next < 0 || next > 108) {
        return;
    }
    LaunchpadX.allNotesOff();
    // The control change comes back through the connection in init, which is
    // what repaints the grid -- here and when the screen moves it instead.
    engine.setValue(LaunchpadX.synthGroup, "base_note", next);
};

/**
 * Step to the next scale, chromatic included: no scale selected is the layout
 * you want before you know the key, so it sits in the cycle rather than being
 * a mode of its own. A mask set elsewhere that is not in the table restarts
 * from the top.
 */
LaunchpadX.cycleScale = function() {
    LaunchpadX.allNotesOff();
    const masks = LaunchpadX.scales.map(function(scale) {
        return LaunchpadX.maskFor(scale.degrees);
    });
    const current = masks.indexOf(LaunchpadX.scaleMask());
    engine.setValue(LaunchpadX.synthGroup, "scale_mask", masks[(current + 1) % masks.length]);
};

/**
 * Root the scale on a note that was played, keeping its pitch class.
 *
 * @param {number} note a MIDI note number
 */
LaunchpadX.setRootFromNote = function(note) {
    LaunchpadX.allNotesOff();
    engine.setValue(LaunchpadX.synthGroup, "scale_root", ((note % 12) + 12) % 12);
};

/**
 * Switch between notes and steps.
 *
 * @param {number} mode MODE_NOTES or MODE_STEPS
 */
LaunchpadX.setMode = function(mode) {
    if (LaunchpadX.mode === mode) {
        return;
    }
    LaunchpadX.allNotesOff();
    LaunchpadX.mode = mode;
    LaunchpadX.draw();
};

/**
 * Release everything the pad is holding.
 */
LaunchpadX.allNotesOff = function() {
    for (const note in LaunchpadX.heldNotes) {
        engine.setValue(LaunchpadX.synthGroup, "note_off", Number(note));
    }
    LaunchpadX.heldNotes = {};
};

/**
 * A top-row or right-column button was pressed or released.
 *
 * @param {number} channel MIDI channel
 * @param {number} control the CC number
 * @param {number} value 127 on press, 0 on release
 */
LaunchpadX.onCC = function(channel, control, value) {
    const rightIndex = LaunchpadX.rightColumn.indexOf(control);
    if (rightIndex !== -1) {
        LaunchpadX.onRightColumn(rightIndex, value > 0);
        return;
    }
    // The scale button acts on RELEASE, because holding it means something
    // else: while it is down a pad sets the root, and then letting go must not
    // also step the scale on.
    if (control === LaunchpadX.CC_SCALE) {
        if (value > 0) {
            LaunchpadX.scaleHeld = true;
            LaunchpadX.scalePickedRoot = false;
        } else {
            LaunchpadX.scaleHeld = false;
            if (!LaunchpadX.scalePickedRoot) {
                LaunchpadX.cycleScale();
            }
        }
        return;
    }
    if (value === 0) {
        return;
    }
    switch (control) {
    case LaunchpadX.CC_NOTES:
        LaunchpadX.setMode(LaunchpadX.MODE_NOTES);
        break;
    case LaunchpadX.CC_STEPS:
        LaunchpadX.setMode(LaunchpadX.MODE_STEPS);
        break;
    case LaunchpadX.CC_OCT_DOWN:
        LaunchpadX.shiftOctave(-1);
        break;
    case LaunchpadX.CC_OCT_UP:
        LaunchpadX.shiftOctave(1);
        break;
    case LaunchpadX.CC_RECORD:
        LaunchpadX.recording = !LaunchpadX.recording;
        LaunchpadX.drawButtons();
        break;
    case LaunchpadX.CC_RUN:
        engine.setValue(LaunchpadX.seqGroup, "run",
            engine.getValue(LaunchpadX.seqGroup, "run") > 0 ? 0 : 1);
        LaunchpadX.drawButtons();
        break;
    case LaunchpadX.CC_CLEAR:
        LaunchpadX.clearPattern();
        break;
    }
};

// --------------------------------------------------------------- lifecycle

/**
 * Put the pad into programmer mode and paint it.
 */
LaunchpadX.init = function() {
    LaunchpadX.selectMode(LaunchpadX.modeProgrammer);
    // Whatever moves the key, the scale or the octave -- these pads or the
    // keyboard on screen -- comes back through here and repaints the grid.
    const shared = ["scale_mask", "scale_root", "base_note"];
    for (let i = 0; i < shared.length; i++) {
        LaunchpadX.connections.push(
            engine.makeConnection(LaunchpadX.synthGroup, shared[i], function() {
                LaunchpadX.drawGrid();
            }));
    }
    LaunchpadX.connections.push(
        engine.makeConnection(LaunchpadX.seqGroup, "current_step", function(value) {
            LaunchpadX.onStepChanged(value);
        }));
    LaunchpadX.connections.push(
        engine.makeConnection(LaunchpadX.seqGroup, "run", function() {
            LaunchpadX.drawButtons();
        }));
    LaunchpadX.draw();
};

/**
 * Let go of the synth, blank the pad and hand it back to Live mode.
 */
LaunchpadX.shutdown = function() {
    LaunchpadX.allNotesOff();
    engine.setValue(LaunchpadX.synthGroup, "all_notes_off", 1);
    for (let i = 0; i < LaunchpadX.connections.length; i++) {
        LaunchpadX.connections[i].disconnect();
    }
    LaunchpadX.connections = [];
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            LaunchpadX.light(LaunchpadX.padIndex(row, col), LaunchpadX.colors.off);
        }
    }
    for (let cc = LaunchpadX.CC_NOTES; cc <= LaunchpadX.CC_CLEAR; cc++) {
        LaunchpadX.light(cc, LaunchpadX.colors.off);
    }
    for (let i = 0; i < LaunchpadX.rightColumn.length; i++) {
        LaunchpadX.light(LaunchpadX.rightColumn[i], LaunchpadX.colors.off);
    }
    LaunchpadX.selectMode(LaunchpadX.modeLive);
};

/**
 * Select the Live or Programmer layout on the pad.
 *
 * @param {number} mode modeLive or modeProgrammer
 */
LaunchpadX.selectMode = function(mode) {
    const msg = LaunchpadX.sysexHeader.concat([0x0E, mode, 0xF7]);
    midi.sendSysexMsg(msg, msg.length);
};
