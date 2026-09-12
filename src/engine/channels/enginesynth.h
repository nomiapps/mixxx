#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include "control/pollingcontrolproxy.h"
#include "engine/channels/enginechannel.h"
#include "engine/channels/wavetable.h"
#include "util/types.h"

class ControlAudioTaperPot;
class ControlObject;
class ControlPotmeter;
class ControlPushButton;

/// EngineSynth is an EngineChannel that generates its own audio instead of
/// reading it from a soundcard input or a track: a small polyphonic
/// subtractive synthesizer. Two oscillators per voice (sine / triangle /
/// PolyBLEP saw / PolyBLEP square, or a frame of a wavetable), a linear
/// ADSR, and a state-variable low-pass filter whose cutoff the envelope
/// can modulate.
///
/// Wave 4 of either oscillator reads the wavetable: wt_position picks the
/// frame, crossfading between neighbours, and the frame is read with linear
/// interpolation. The table itself arrives from the main thread down a
/// lock-free lane (see wavetable.h); until one has arrived wave 4 plays the
/// saw, so the channel is never silent by surprise. Unlike the PolyBLEP
/// waves a table is not band-limited by nature, so each frame comes in
/// mips (see wavetable.h) and a voice reads the first one whose partials
/// all sit below Nyquist for its pitch, chosen per control-rate block.
///
/// One LFO per synth modulates the wavetable position, the cutoff or the
/// pitch of every voice (lfo_target), with lfo_shape, lfo_depth and
/// lfo_rate. With lfo_sync its phase is derived from the [InternalClock]
/// beat position every buffer, so it locks to the mix and cannot drift;
/// free-running, lfo_rate is 0.05 .. 20 Hz. The value is evaluated once
/// per control-rate block, the same block that recomputes the filter, and
/// is stateless in the voice: every voice reads the same LFO. lfo_phase
/// (read-only) publishes the phase for a display cursor.
///
/// Unison starts unison_voices (1..4) voices per note, spread symmetrically
/// by up to unison_detune (0..50 cents) and panned apart by unison_spread.
/// Each has its own filter and phases, which is what makes them beat. The
/// output is stereo from here on: voices sum into a left and a right
/// buffer with a no-boost pan law (centre 1.0 both sides), scaled by
/// 1/sqrt(voices) so a note is about as loud however many it uses.
///
/// It is played through controls in its group ("[Synth1]"):
///   note_on   set to a MIDI note number to start it (a fractional part
///             carries velocity: note + velocity / 128, so a bare integer is
///             full velocity)
///   note_off  set to a MIDI note number to release it
///   all_notes_off  release everything
/// Key state lives in per-note atomics that any thread may set; the engine
/// thread diffs them once per buffer, so no locks and no allocation happen on
/// the audio path.
class EngineSynth : public EngineChannel {
    Q_OBJECT
  public:
    // Sixteen, so four-note chords at four-voice unison do not steal from
    // each other. A voice is two oscillators and a filter; sixteen is
    // still far below one deck's time stretcher.
    static constexpr int kVoices = 16;
    static constexpr int kMaxUnison = 4;
    // The envelope knobs' top ends, in seconds. Public with the mappings
    // below so a display draws exactly what the engine will do.
    static constexpr double kMaxAttackSeconds = 4.0;
    static constexpr double kMaxDecaySeconds = 4.0;
    static constexpr double kMaxReleaseSeconds = 8.0;

    /// 0..1 -> 1 ms .. maxSeconds, exponentially, so the bottom of the
    /// knob is usable for percussive settings.
    static double envelopeSeconds(double param, double maxSeconds);
    /// The cutoff knob, 0..1, in Hz: 20 Hz to 20 kHz, exponentially.
    static double cutoffHz(double param);
    /// The low-pass filter's gain in dB at hz for a cutoff and resonance
    /// knob setting: the two-pole response the engine's filter is tuned
    /// to (it prewarps to match it), so a curve drawn from this is what
    /// the ear gets.
    static double filterResponseDb(double cutoffParam, double resonanceParam, double hz);
    static constexpr int kNotes = 128;

    EngineSynth(const ChannelHandleAndGroup& handleGroup, EffectsManager* pEffectsManager);
    ~EngineSynth() override;

    ActiveState updateActiveState() override;

    /// Called by EngineMixer whenever it requests a new buffer of audio.
    void process(CSAMPLE* pOutput, const std::size_t bufferSize) override;
    void collectFeatures(GroupFeatureState* pGroupFeatures) const override;

    /// Thread-safe note input. velocity is 0..1; a value <= 0 means full.
    void noteOn(int note, double velocity = 1.0);
    void noteOff(int note);
    void allNotesOff();
    bool isNoteHeld(int note) const;

    /// Number of voices currently sounding (for tests and diagnostics; read
    /// from the engine thread's point of view, so only approximate elsewhere).
    int activeVoiceCount() const;

    /// Engine-thread-only note scheduling, for callers that themselves run
    /// inside the engine callback (the step sequencer). The key controls above
    /// are diffed once per buffer, so anything they play lands at frame 0 of
    /// the next buffer and two events for one note in one buffer collapse.
    /// These do not: an event is applied by the next process() call at
    /// frameOffset frames into its buffer, between the samples on either side
    /// of it. frameOffset past the end of the buffer lands on its last frame.
    /// velocity is 0..1. Returns false when the queue is full and the event
    /// was dropped.
    static constexpr int kMaxScheduledEvents = 64;
    bool scheduleNoteOn(std::size_t frameOffset, int note, double velocity);
    bool scheduleNoteOff(std::size_t frameOffset, int note);
    /// Events queued for the next process() (tests and diagnostics).
    int scheduledEventCount() const;

    /// Main-thread setup, before the channel is handed to EngineMixer: the
    /// engine end of the lane new wavetables come down.
    void setWavetablePipe(WavetableEnginePipe&& pipe);
    /// Engine thread only (process() calls it for the lane; tests call it
    /// directly). Takes ownership of pTable, nullptr clearing the table, and
    /// returns the previous one, which the caller now owns and must free
    /// somewhere other than the engine thread.
    Wavetable* adoptWavetable(Wavetable* pTable);
    /// The table the engine is playing, from the engine thread's point of
    /// view (tests and diagnostics).
    const Wavetable* wavetable() const {
        return m_pTable;
    }

    /// The LFO's value, -1..1, for a shape (0 sine, 1 triangle, 2 saw down,
    /// 3 square, 4 sample and hold) at an absolute phase whose integer part
    /// counts cycles: sample and hold hashes the cycle number, so it needs no
    /// state either. Static so the display draws the engine's own shapes.
    static double lfoValue(int shape, double absolutePhase);
    /// The mip a voice at this phase increment (cycles per sample) reads:
    /// the first whose partials all fit below Nyquist, clamped to what the
    /// table has.
    static int mipForIncrement(double inc, int mipCount);

  private slots:
    void slotNoteOn(double v);
    void slotNoteOff(double v);
    void slotAllNotesOff(double v);

  private:
    enum class Stage {
        Idle = 0,
        Attack,
        Decay,
        Sustain,
        Release,
    };

    struct Voice {
        int note = -1;
        uint32_t startedAt = 0;
        Stage stage = Stage::Idle;
        bool gate = false;
        double velocity = 1.0;
        double env = 0.0;
        double phase1 = 0.0;
        double phase2 = 0.0;
        double ic1eq = 0.0;
        double ic2eq = 0.0;
        // Which of the note's unison voices this is, and how many the note
        // started with. Detune and spread are read per buffer, so the knobs
        // move held notes; only the index and count are baked at note-on.
        uint8_t unisonIndex = 0;
        uint8_t unisonCount = 1;
    };

    /// Parameters sampled once per buffer from the controls.
    struct Params {
        double sampleRate = 44100.0;
        int wave1 = 2;
        int wave2 = 2;
        double oscMix = 0.5;
        double osc2Ratio = 1.0;
        double attackInc = 0.0;
        double decayInc = 0.0;
        double sustain = 1.0;
        double releaseInc = 0.0;
        double cutoffHz = 5000.0;
        double damping = 1.0;
        double envAmountOctaves = 0.0;
        double gain = 1.0;
        // Wave 4 reads frames wtFrameA and wtFrameB of pWavetable, blended
        // wtBlend of the way from A to B. Never wave 4 with no table.
        const Wavetable* pWavetable = nullptr;
        int wtFrameA = 0;
        int wtFrameB = 0;
        double wtBlend = 0.0;
        // The unmodulated knob, 0..1, for the LFO to move.
        double wtPosition = 0.0;
        // The LFO for this buffer: shape and target, depth already applied
        // to the value, and the absolute phase at frame 0 plus its
        // increment per frame.
        int lfoShape = 0;
        int lfoTarget = 0;
        double lfoDepth = 0.0;
        double lfoPhase0 = 0.0;
        double lfoInc = 0.0;
        int unisonVoices = 1;
        double unisonCents = 0.0;
        double unisonSpread = 0.0;
    };

    void readParams(Params* pParams) const;
    void applyKeyChanges();
    void startVoice(int note, double velocity, bool gate);
    void releaseVoicesFor(int note);
    Voice* findVoiceFor(int note, int unisonIndex);
    Voice* allocateVoice();
    /// bufferOffset is where pMono sits in the buffer, so a voice can place
    /// itself on the LFO's timeline; scheduled events split a buffer into
    /// segments that start anywhere.
    void renderVoice(Voice* pVoice,
            const Params& params,
            CSAMPLE* pLeft,
            CSAMPLE* pRight,
            std::size_t bufferOffset,
            std::size_t frames);
    void drainWavetableLane();
    void advanceBeatClock();

    struct ScheduledEvent {
        uint32_t frame = 0;
        uint8_t note = 0;
        uint8_t velocity = 127;
        bool on = false;
    };

    std::array<std::atomic<uint64_t>, 2> m_held;
    std::array<std::atomic<uint64_t>, 2> m_tapped;
    std::array<std::atomic<uint8_t>, kNotes> m_velocity;
    std::array<uint64_t, 2> m_lastHeld;
    std::array<Voice, kVoices> m_voices;
    uint32_t m_voiceSequence;
    std::vector<CSAMPLE> m_leftBuffer;
    std::vector<CSAMPLE> m_rightBuffer;
    // unison_voices as read for this buffer, for the note starts in it.
    int m_unisonVoices;
    // Engine thread only: filled between callbacks by the sequencer, drained
    // by process(). Sorted by frame there, so callers need not order them.
    std::array<ScheduledEvent, kMaxScheduledEvents> m_scheduled;
    int m_scheduledCount;
    // Engine thread only, apart from the destructor, which runs after the
    // audio callback has stopped.
    Wavetable* m_pTable;
    std::optional<WavetableEnginePipe> m_wavetablePipe;
    // The beat clock, read once per callback in updateActiveState() so
    // bars are counted while the synth is silent too. beat_distance is
    // only the phase inside a beat; the rollovers are counted here. With no
    // clock in the process (a synth built without a mixer) the proxies
    // read a default control: bpm falls back to 124 and the phase stays 0.
    PollingControlProxy m_clockBpm;
    PollingControlProxy m_clockBeatDistance;
    bool m_clockPrimed;
    double m_prevBeatPhase;
    int64_t m_beatCount;
    double m_beatsAtCallback;
    double m_beatFrames;
    // Free-running mode: absolute phase in cycles, advanced per buffer.
    double m_lfoFreePhase;
    double m_lastPublishedPhase;

    ControlObject* m_pNoteOn;
    ControlObject* m_pNoteOff;
    ControlPushButton* m_pAllNotesOff;
    ControlObject* m_pOsc1Wave;
    ControlObject* m_pOsc2Wave;
    ControlPotmeter* m_pOscMix;
    ControlPotmeter* m_pWtPosition;
    ControlPotmeter* m_pOsc2Semitones;
    ControlPotmeter* m_pOsc2Detune;
    ControlPotmeter* m_pAttack;
    ControlPotmeter* m_pDecay;
    ControlPotmeter* m_pSustain;
    ControlPotmeter* m_pRelease;
    ControlPotmeter* m_pCutoff;
    ControlPotmeter* m_pResonance;
    ControlPotmeter* m_pEnvAmount;
    ControlAudioTaperPot* m_pPregain;
    ControlObject* m_pLfoShape;
    ControlPotmeter* m_pLfoRate;
    ControlPushButton* m_pLfoSync;
    ControlPotmeter* m_pLfoDepth;
    ControlObject* m_pLfoTarget;
    ControlObject* m_pLfoPhase;
    ControlObject* m_pUnisonVoices;
    ControlPotmeter* m_pUnisonDetune;
    ControlPotmeter* m_pUnisonSpread;
};
