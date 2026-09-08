#pragma once

#include <QObject>
#include <QString>
#include <array>
#include <atomic>
#include <bitset>
#include <cstddef>
#include <cstdint>

#include "audio/types.h"
#include "control/pollingcontrolproxy.h"

class ControlObject;
class ControlPotmeter;
class ControlPushButton;
class EngineBuffer;
class EngineMixer;
class EngineSynth;

/// EngineSequencer is a 16-step pattern sequencer that runs inside the engine
/// callback. One pitched lane plays the built-in synth ([Synth1]) with a note,
/// velocity and gate per step; four drum lanes each fire a sampler deck on
/// their enabled steps. Steps are 16th notes locked to the sync leader's beat,
/// read from the InternalClock, which follows whichever deck leads and
/// free-runs at its own tempo when nothing plays.
///
/// It is not a channel: it makes no audio of its own. EngineMixer owns it and
/// ticks it once per callback BEFORE any channel is processed, so the notes
/// it schedules are seen by the synth's active-state check and a sampler it
/// starts plays in the same buffer. Synth notes land on their exact frame
/// (EngineSynth::scheduleNoteOn); sampler starts land on the buffer.
///
/// Controls, group "[Sequencer1]":
///   run, restart, swing (0..1), length (1..16), current_step (read-only)
///   synth_step_N_{enabled,note,velocity,gate}          N = 1..16
///   sampler_L_step_N_enabled, sampler_L_target         L = 1..4
/// The pattern controls persist in the config, so a pattern survives a
/// restart. Nothing here allocates or locks on the engine thread.
class EngineSequencer : public QObject {
    Q_OBJECT
  public:
    static constexpr int kSteps = 16;
    static constexpr int kStepsPerBeat = 4;
    static constexpr int kSamplerLanes = 4;
    static constexpr int kMaxSamplerTargets = 16;

    /// The beat clock as read at the start of a callback: the phase within
    /// the current beat (0..1) at frame 0 of the buffer, and the tempo.
    struct Clock {
        double beatDistance;
        double bpm;
    };

    /// pMixer and pSynth may each be null; that side is then inert. The clock
    /// controls need not exist either, since tests drive tick() directly.
    EngineSequencer(const QString& group, EngineMixer* pMixer, EngineSynth* pSynth);
    ~EngineSequencer() override;

    /// Called by EngineMixer at the top of every callback, before any
    /// channel. bufferSize is in interleaved stereo samples.
    void onCallbackStart(mixxx::audio::SampleRate sampleRate, std::size_t bufferSize);

    /// The engine-thread entry onCallbackStart() reaches after reading the
    /// clock controls. Public so tests can feed synthetic phases. frames, not
    /// samples.
    void tick(const Clock& clock, mixxx::audio::SampleRate sampleRate, std::size_t frames);

    /// Diagnostics and tests; engine-thread values.
    bool isRunning() const {
        return m_running;
    }
    /// Sampler events dispatched for a lane, whether or not a deck was there
    /// to receive them.
    int samplerFireCount(int lane) const;

  private slots:
    void slotRestart(double v);

  private:
    enum class EventKind : uint8_t {
        NoteOn,
        NoteOff,
        Sampler,
    };

    struct Event {
        uint64_t frame = 0; // absolute: frames since the sequencer was created
        EventKind kind = EventKind::NoteOn;
        uint8_t note = 0;
        uint8_t velocity = 0; // 0..127
        uint8_t lane = 0;
    };

    /// The transport controls, snapshotted once per buffer.
    struct Transport {
        bool run = false;
        double swing = 0.0;
        int length = kSteps;
    };

    struct SamplerLane {
        EngineBuffer* pBuffer = nullptr;
        int cachedTarget = -1;
        int cachedNumSamplers = -1;
        int fireCount = 0;
    };

    // A boundary yields at most 2 + kSamplerLanes events and a buffer holds a
    // handful of boundaries; the rest is note-offs carried across buffers.
    static constexpr int kMaxPendingEvents = 64;

    void readTransport(Transport* pTransport) const;
    void onBoundary(int64_t boundary, uint64_t onFrame, double stepFrames, const Transport& transport);
    void dispatchPending(uint64_t bufferStart, uint64_t bufferEnd);
    void stop();
    bool push(const Event& event);
    void cancelPendingNoteOff(int note);
    void fireSampler(int lane);
    EngineBuffer* resolveSampler(int lane);

    const QString m_group;
    EngineMixer* m_pMixer;
    EngineSynth* m_pSynth;

    PollingControlProxy m_clockBpm;
    PollingControlProxy m_clockBeatDistance;
    PollingControlProxy m_numSamplers;

    ControlPushButton* m_pRun;
    ControlPushButton* m_pRestart;
    ControlPotmeter* m_pSwing;
    ControlObject* m_pLength;
    ControlObject* m_pCurrentStep;
    std::array<ControlPushButton*, kSteps> m_synthEnabled;
    std::array<ControlObject*, kSteps> m_synthNote;
    std::array<ControlPotmeter*, kSteps> m_synthVelocity;
    std::array<ControlPotmeter*, kSteps> m_synthGate;
    std::array<std::array<ControlPushButton*, kSteps>, kSamplerLanes> m_samplerEnabled;
    std::array<ControlObject*, kSamplerLanes> m_samplerTarget;
    // Built once in the constructor: the engine thread never builds strings.
    std::array<QString, kMaxSamplerTargets> m_samplerGroups;
    std::array<SamplerLane, kSamplerLanes> m_lanes;

    // Set from whichever thread writes the control; consumed on the engine thread.
    std::atomic<bool> m_restartRequested;

    // Engine thread only from here on.
    bool m_running;
    int m_currentStep; // the next step to fire; -1 while waiting for a beat
    bool m_clockPrimed;
    double m_prevPhase;
    int64_t m_beatCount;
    double m_predictedA0;
    int64_t m_nextBoundary;
    uint64_t m_frameCounter;
    std::array<Event, kMaxPendingEvents> m_pending;
    int m_pendingCount;
    std::bitset<128> m_gated; // synth notes the sequencer is holding down
};
