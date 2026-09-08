#include "engine/enginesequencer.h"

#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlpushbutton.h"
#include "engine/channels/enginechannel.h"
#include "engine/channels/enginesynth.h"
#include "engine/enginebuffer.h"
#include "engine/enginemixer.h"
#include "mixer/playermanager.h"
#include "moc_enginesequencer.cpp"
#include "util/assert.h"

namespace {

constexpr double kDefaultBpm = 124.0;
constexpr double kMinBpm = 1.0;
constexpr double kMaxBpm = 400.0;
constexpr int kDefaultNote = 48; // C3, where the synth keyboard also starts
constexpr double kMinGate = 0.05;
constexpr double kDefaultGate = 0.5;
// Where a buffer starts, in steps, is predicted from the previous buffer. A
// discrepancy this large is a seek, a leader change or a tempo jump, not
// float drift; the pattern then resumes at the next boundary instead of
// firing every step it "missed" at once.
constexpr double kResyncThresholdSteps = 0.5;

QString synthStepKey(int step, const char* field) {
    return QStringLiteral("synth_step_%1_%2").arg(step + 1).arg(QLatin1String(field));
}

QString samplerStepKey(int lane, int step) {
    return QStringLiteral("sampler_%1_step_%2_enabled").arg(lane + 1).arg(step + 1);
}

} // namespace

EngineSequencer::EngineSequencer(const QString& group, EngineMixer* pMixer, EngineSynth* pSynth)
        : m_group(group),
          m_pMixer(pMixer),
          m_pSynth(pSynth),
          m_clockBpm(QStringLiteral("[InternalClock]"),
                  QStringLiteral("bpm"),
                  ControlFlag::AllowMissingOrInvalid),
          m_clockBeatDistance(QStringLiteral("[InternalClock]"),
                  QStringLiteral("beat_distance"),
                  ControlFlag::AllowMissingOrInvalid),
          m_numSamplers(QStringLiteral("[App]"),
                  QStringLiteral("num_samplers"),
                  ControlFlag::AllowMissingOrInvalid),
          m_restartRequested(false),
          m_running(false),
          m_currentStep(-1),
          m_clockPrimed(false),
          m_prevPhase(0.0),
          m_beatCount(0),
          m_predictedA0(0.0),
          m_nextBoundary(0),
          m_frameCounter(0),
          m_pendingCount(0) {
    m_pRun = new ControlPushButton(ConfigKey(group, QStringLiteral("run")));
    m_pRun->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pRestart = new ControlPushButton(ConfigKey(group, QStringLiteral("restart")));
    connect(m_pRestart,
            &ControlObject::valueChanged,
            this,
            &EngineSequencer::slotRestart,
            Qt::DirectConnection);
    m_pSwing = new ControlPotmeter(ConfigKey(group, QStringLiteral("swing")),
            0.0,
            1.0,
            /*allowOutOfBounds*/ false,
            /*bIgnoreNops*/ true,
            /*bTrack*/ false,
            /*bPersist*/ true,
            0.0);
    m_pLength = new ControlObject(ConfigKey(group, QStringLiteral("length")),
            /*bIgnoreNops*/ true,
            /*bTrack*/ false,
            /*bPersist*/ true,
            kSteps);
    m_pCurrentStep = new ControlObject(ConfigKey(group, QStringLiteral("current_step")),
            /*bIgnoreNops*/ true,
            /*bTrack*/ false,
            /*bPersist*/ false,
            -1.0);
    m_pCurrentStep->setReadOnly();
    m_pCurrentStep->setAndConfirm(-1.0);

    for (int step = 0; step < kSteps; ++step) {
        m_synthEnabled[step] = new ControlPushButton(
                ConfigKey(group, synthStepKey(step, "enabled")), /*bPersist*/ true, 0.0);
        m_synthEnabled[step]->setButtonMode(mixxx::control::ButtonMode::Toggle);
        m_synthNote[step] = new ControlObject(ConfigKey(group, synthStepKey(step, "note")),
                true,
                false,
                /*bPersist*/ true,
                kDefaultNote);
        m_synthVelocity[step] = new ControlPotmeter(
                ConfigKey(group, synthStepKey(step, "velocity")),
                0.0,
                1.0,
                false,
                true,
                false,
                /*bPersist*/ true,
                1.0);
        m_synthGate[step] = new ControlPotmeter(ConfigKey(group, synthStepKey(step, "gate")),
                kMinGate,
                1.0,
                false,
                true,
                false,
                /*bPersist*/ true,
                kDefaultGate);
        for (int lane = 0; lane < kSamplerLanes; ++lane) {
            m_samplerEnabled[lane][step] = new ControlPushButton(
                    ConfigKey(group, samplerStepKey(lane, step)), /*bPersist*/ true, 0.0);
            m_samplerEnabled[lane][step]->setButtonMode(mixxx::control::ButtonMode::Toggle);
        }
    }
    for (int lane = 0; lane < kSamplerLanes; ++lane) {
        m_samplerTarget[lane] = new ControlObject(
                ConfigKey(group, QStringLiteral("sampler_%1_target").arg(lane + 1)),
                true,
                false,
                /*bPersist*/ true,
                lane + 1);
    }
    for (int target = 0; target < kMaxSamplerTargets; ++target) {
        m_samplerGroups[target] = PlayerManager::groupForSampler(target);
    }
}

EngineSequencer::~EngineSequencer() {
    for (int lane = 0; lane < kSamplerLanes; ++lane) {
        delete m_samplerTarget[lane];
        for (int step = 0; step < kSteps; ++step) {
            delete m_samplerEnabled[lane][step];
        }
    }
    for (int step = 0; step < kSteps; ++step) {
        delete m_synthGate[step];
        delete m_synthVelocity[step];
        delete m_synthNote[step];
        delete m_synthEnabled[step];
    }
    delete m_pCurrentStep;
    delete m_pLength;
    delete m_pSwing;
    delete m_pRestart;
    delete m_pRun;
}

int EngineSequencer::samplerFireCount(int lane) const {
    if (lane < 0 || lane >= kSamplerLanes) {
        return 0;
    }
    return m_lanes[lane].fireCount;
}

void EngineSequencer::slotRestart(double v) {
    if (v > 0.0) {
        m_restartRequested.store(true, std::memory_order_release);
    }
}

void EngineSequencer::readTransport(Transport* pTransport) const {
    pTransport->run = m_pRun->toBool();
    pTransport->swing = std::clamp(m_pSwing->get(), 0.0, 1.0);
    pTransport->length = std::clamp(static_cast<int>(std::lround(m_pLength->get())), 1, kSteps);
}

void EngineSequencer::onCallbackStart(mixxx::audio::SampleRate sampleRate, std::size_t bufferSize) {
    // bufferSize is interleaved stereo samples; the clock works in frames.
    tick(Clock{m_clockBeatDistance.get(), m_clockBpm.get()}, sampleRate, bufferSize / 2);
}

void EngineSequencer::tick(const Clock& clock, mixxx::audio::SampleRate sampleRate, std::size_t frames) {
    Transport transport;
    readTransport(&transport);

    double sr = sampleRate.toDouble();
    if (sr <= 0.0) {
        sr = 44100.0;
    }
    double bpm = clock.bpm;
    if (!(bpm >= kMinBpm)) { // also NaN, and 0 from a missing control
        bpm = kDefaultBpm;
    }
    bpm = std::min(bpm, kMaxBpm);
    // The same arithmetic as InternalClock::updateBeatLength.
    const double beatFrames = sr * 60.0 / bpm;
    const double stepFrames = beatFrames / kStepsPerBeat;
    double phase = clock.beatDistance;
    if (!std::isfinite(phase)) {
        phase = 0.0;
    }
    phase -= std::floor(phase); // into [0, 1)

    const uint64_t bufferStart = m_frameCounter;
    const uint64_t bufferEnd = bufferStart + frames;

    if (transport.run && !m_running) {
        m_running = true;
        m_currentStep = -1; // the first step lands on the next beat
    } else if (!transport.run && m_running) {
        stop();
    }

    if (!m_clockPrimed) {
        m_clockPrimed = true;
        m_prevPhase = phase;
        m_beatCount = 0;
        const double first = phase * kStepsPerBeat;
        m_nextBoundary = static_cast<int64_t>(std::ceil(first));
        m_predictedA0 = first;
    }
    // The clock reports only the phase within the beat; count its rollovers
    // to get a monotonic timeline in steps. A large backward jump is a beat.
    if (phase < m_prevPhase - 0.5) {
        ++m_beatCount;
    }
    m_prevPhase = phase;
    const double a0 = (static_cast<double>(m_beatCount) + phase) * kStepsPerBeat;
    const double aEnd = a0 + static_cast<double>(frames) / stepFrames;
    if (std::fabs(a0 - m_predictedA0) > kResyncThresholdSteps) {
        // Forward jump: skip what was missed. Backward jump: wait for the
        // timeline to catch up rather than fire a boundary twice.
        m_nextBoundary = std::max(m_nextBoundary, static_cast<int64_t>(std::ceil(a0)));
    }
    m_predictedA0 = aEnd;

    // Every step boundary inside [a0, aEnd), each at its own frame. Half-open
    // and integer-tracked, so a boundary on a buffer seam fires exactly once.
    for (int64_t boundary = m_nextBoundary; static_cast<double>(boundary) < aEnd; ++boundary) {
        const double offset = std::max(0.0, (static_cast<double>(boundary) - a0) * stepFrames);
        const uint64_t last = frames > 0 ? frames - 1 : 0;
        const uint64_t onFrame = bufferStart + std::min<uint64_t>(static_cast<uint64_t>(offset), last);
        onBoundary(boundary, onFrame, stepFrames, transport);
        m_nextBoundary = boundary + 1;
    }

    dispatchPending(bufferStart, bufferEnd);
    m_frameCounter = bufferEnd;
}

void EngineSequencer::onBoundary(int64_t boundary,
        uint64_t onFrame,
        double stepFrames,
        const Transport& transport) {
    if (!m_running) {
        return;
    }
    const bool beat = ((boundary % kStepsPerBeat) + kStepsPerBeat) % kStepsPerBeat == 0;
    if (m_currentStep < 0) {
        // Just started: step 0 goes on a beat, not on whatever 16th is next.
        if (!beat) {
            return;
        }
        m_currentStep = 0;
    } else if (beat && m_restartRequested.exchange(false, std::memory_order_acq_rel)) {
        m_currentStep = 0;
    }
    const int step = m_currentStep % transport.length;

    // Swing pushes the odd steps late: 0 is straight, 1 is a 75% shuffle.
    const double swingFrames = (step & 1) ? transport.swing * 0.5 * stepFrames : 0.0;
    const uint64_t at = onFrame + static_cast<uint64_t>(swingFrames);

    if (m_pSynth && m_synthEnabled[step]->toBool()) {
        const int note = std::clamp(
                static_cast<int>(std::lround(m_synthNote[step]->get())), 0, EngineSynth::kNotes - 1);
        const double velocity = std::clamp(m_synthVelocity[step]->get(), 0.0, 1.0);
        const double gate = std::clamp(m_synthGate[step]->get(), kMinGate, 1.0);
        // A step retriggering a note that is still gated must not be cut off
        // by that earlier gate.
        cancelPendingNoteOff(note);
        Event on;
        on.frame = at;
        on.kind = EventKind::NoteOn;
        on.note = static_cast<uint8_t>(note);
        on.velocity = static_cast<uint8_t>(std::lround(velocity * 127.0));
        push(on);
        Event off;
        off.frame = at + std::max<uint64_t>(1, static_cast<uint64_t>(gate * stepFrames));
        off.kind = EventKind::NoteOff;
        off.note = static_cast<uint8_t>(note);
        push(off);
    }
    for (int lane = 0; lane < kSamplerLanes; ++lane) {
        if (m_samplerEnabled[lane][step]->toBool()) {
            Event hit;
            hit.frame = at;
            hit.kind = EventKind::Sampler;
            hit.lane = static_cast<uint8_t>(lane);
            push(hit);
        }
    }

    m_pCurrentStep->setAndConfirm(step);
    m_currentStep = (step + 1) % transport.length;
}

void EngineSequencer::dispatchPending(uint64_t bufferStart, uint64_t bufferEnd) {
    for (int i = 0; i < m_pendingCount;) {
        const Event& event = m_pending[i];
        if (event.frame >= bufferEnd) {
            ++i;
            continue;
        }
        const std::size_t offset = event.frame > bufferStart
                ? static_cast<std::size_t>(event.frame - bufferStart)
                : 0;
        switch (event.kind) {
        case EventKind::NoteOn:
            if (m_pSynth) {
                m_pSynth->scheduleNoteOn(offset, event.note, event.velocity / 127.0);
                m_gated.set(event.note);
            }
            break;
        case EventKind::NoteOff:
            if (m_pSynth) {
                m_pSynth->scheduleNoteOff(offset, event.note);
            }
            m_gated.reset(event.note);
            break;
        case EventKind::Sampler:
            // Buffer-accurate: the deck starts at the top of this buffer.
            fireSampler(event.lane);
            break;
        }
        // Unordered removal; the synth sorts its events by frame itself.
        m_pending[i] = m_pending[--m_pendingCount];
    }
}

void EngineSequencer::stop() {
    m_running = false;
    m_pendingCount = 0;
    if (m_pSynth) {
        for (int note = 0; note < static_cast<int>(m_gated.size()); ++note) {
            if (m_gated.test(note)) {
                m_pSynth->scheduleNoteOff(0, note);
            }
        }
    }
    m_gated.reset();
    m_currentStep = -1;
}

bool EngineSequencer::push(const Event& event) {
    VERIFY_OR_DEBUG_ASSERT(m_pendingCount < kMaxPendingEvents) {
        return false;
    }
    m_pending[m_pendingCount++] = event;
    return true;
}

void EngineSequencer::cancelPendingNoteOff(int note) {
    for (int i = 0; i < m_pendingCount;) {
        if (m_pending[i].kind == EventKind::NoteOff && m_pending[i].note == note) {
            m_pending[i] = m_pending[--m_pendingCount];
        } else {
            ++i;
        }
    }
}

void EngineSequencer::fireSampler(int lane) {
    ++m_lanes[lane].fireCount;
    EngineBuffer* pBuffer = resolveSampler(lane);
    if (pBuffer) {
        // Not slotControlPlayFromStart: with quantize on, its play request
        // queues a phase seek that would move the start off frame 0.
        pBuffer->playFromStartUnquantized();
    }
}

EngineBuffer* EngineSequencer::resolveSampler(int lane) {
    if (!m_pMixer) {
        return nullptr;
    }
    SamplerLane& state = m_lanes[lane];
    const int target = std::clamp(
            static_cast<int>(std::lround(m_samplerTarget[lane]->get())), 1, kMaxSamplerTargets);
    // Samplers can be added after startup (the skin sets [App] num_samplers),
    // so the count is part of the cache key. getChannel is a lock-free scan.
    const int numSamplers = static_cast<int>(m_numSamplers.get());
    if (target != state.cachedTarget || numSamplers != state.cachedNumSamplers) {
        state.cachedTarget = target;
        state.cachedNumSamplers = numSamplers;
        EngineChannel* pChannel = m_pMixer->getChannel(m_samplerGroups[target - 1]);
        state.pBuffer = pChannel ? pChannel->getEngineBuffer() : nullptr;
    }
    return state.pBuffer;
}
