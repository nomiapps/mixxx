#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "engine/channels/enginechannel.h"
#include "util/types.h"

class ControlAudioTaperPot;
class ControlObject;
class ControlPotmeter;
class ControlPushButton;

/// EngineSynth is an EngineChannel that generates its own audio instead of
/// reading it from a soundcard input or a track: a small polyphonic
/// subtractive synthesizer. Two oscillators per voice (sine / triangle /
/// PolyBLEP saw / PolyBLEP square), a linear ADSR, and a state-variable
/// low-pass filter whose cutoff the envelope can modulate.
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
    static constexpr int kVoices = 8;
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
    };

    void readParams(Params* pParams) const;
    void applyKeyChanges();
    void startVoice(int note, double velocity, bool gate);
    void releaseVoicesFor(int note);
    Voice* findVoiceFor(int note);
    Voice* allocateVoice();
    void renderVoice(Voice* pVoice, const Params& params, CSAMPLE* pMono, std::size_t frames);

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
    std::vector<CSAMPLE> m_monoBuffer;
    // Engine thread only: filled between callbacks by the sequencer, drained
    // by process(). Sorted by frame there, so callers need not order them.
    std::array<ScheduledEvent, kMaxScheduledEvents> m_scheduled;
    int m_scheduledCount;

    ControlObject* m_pNoteOn;
    ControlObject* m_pNoteOff;
    ControlPushButton* m_pAllNotesOff;
    ControlObject* m_pOsc1Wave;
    ControlObject* m_pOsc2Wave;
    ControlPotmeter* m_pOscMix;
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
};
