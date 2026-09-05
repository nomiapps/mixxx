#include "engine/channels/enginesynth.h"

#include <algorithm>
#include <cmath>

#include "control/controlaudiotaperpot.h"
#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlpushbutton.h"
#include "effects/effectsmanager.h"
#include "engine/effects/engineeffectsmanager.h"
#include "moc_enginesynth.cpp"
#include "util/assert.h"
#include "util/defs.h"
#include "util/sample.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kMinCutoffHz = 20.0;
constexpr double kMaxCutoffHz = 20000.0;
// Eight full-velocity voices summed at this level stay under 0 dBFS with a
// resonant filter adding a few dB on top.
constexpr double kVoiceLevel = 0.2;
// The filter coefficients follow the envelope, but recomputing a tan() for
// every sample of every voice is wasteful; this is inaudible at 44.1 kHz.
constexpr int kControlRateSamples = 32;
constexpr double kMaxAttackSeconds = 4.0;
constexpr double kMaxDecaySeconds = 4.0;
constexpr double kMaxReleaseSeconds = 8.0;
constexpr double kFilterEnvelopeOctaves = 5.0;

// Two-sample polynomial band-limited step, subtracted from a naive saw or
// square at each discontinuity to remove most of the aliasing.
double polyBlep(double t, double dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

double oscillator(int wave, double phase, double inc) {
    switch (wave) {
    case 0: // sine
        return std::sin(kTwoPi * phase);
    case 1: // triangle
        return 4.0 * std::fabs(phase - 0.5) - 1.0;
    case 3: { // square
        double value = phase < 0.5 ? 1.0 : -1.0;
        value += polyBlep(phase, inc);
        double shifted = phase + 0.5;
        if (shifted >= 1.0) {
            shifted -= 1.0;
        }
        value -= polyBlep(shifted, inc);
        return value;
    }
    case 2: // saw
    default:
        return 2.0 * phase - 1.0 - polyBlep(phase, inc);
    }
}

// 0..1 -> 1 ms .. maxSeconds, exponentially, so the bottom of the knob is
// usable for percussive settings.
double envelopeSeconds(double param, double maxSeconds) {
    param = std::clamp(param, 0.0, 1.0);
    return 0.001 * std::pow(maxSeconds * 1000.0, param);
}

double noteToHz(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

} // namespace

EngineSynth::EngineSynth(const ChannelHandleAndGroup& handleGroup, EffectsManager* pEffectsManager)
        : EngineChannel(handleGroup,
                  EngineChannel::CENTER,
                  pEffectsManager,
                  /*isTalkoverChannel*/ false,
                  /*isPrimaryDeck*/ false),
          m_lastHeld{0, 0},
          m_voiceSequence(0),
          m_monoBuffer(kMaxEngineFrames, 0.0f) {
    for (auto& held : m_held) {
        held.store(0, std::memory_order_relaxed);
    }
    for (auto& tapped : m_tapped) {
        tapped.store(0, std::memory_order_relaxed);
    }
    for (auto& velocity : m_velocity) {
        velocity.store(127, std::memory_order_relaxed);
    }

    // Note input. ignoreNops is off so playing the same note twice in a row
    // still reaches the slot; the value is the MIDI note number.
    m_pNoteOn = new ControlObject(ConfigKey(getGroup(), "note_on"), /*bIgnoreNops*/ false);
    connect(m_pNoteOn,
            &ControlObject::valueChanged,
            this,
            &EngineSynth::slotNoteOn,
            Qt::DirectConnection);
    m_pNoteOff = new ControlObject(ConfigKey(getGroup(), "note_off"), /*bIgnoreNops*/ false);
    connect(m_pNoteOff,
            &ControlObject::valueChanged,
            this,
            &EngineSynth::slotNoteOff,
            Qt::DirectConnection);
    m_pAllNotesOff = new ControlPushButton(ConfigKey(getGroup(), "all_notes_off"));
    connect(m_pAllNotesOff,
            &ControlObject::valueChanged,
            this,
            &EngineSynth::slotAllNotesOff,
            Qt::DirectConnection);

    // Oscillators: 0 sine, 1 triangle, 2 saw, 3 square. Plain values rather
    // than push buttons so a surface or a mapping can set them directly.
    m_pOsc1Wave = new ControlObject(ConfigKey(getGroup(), "osc1_wave"));
    m_pOsc1Wave->setDefaultValue(2.0);
    m_pOsc1Wave->set(2.0);
    m_pOsc2Wave = new ControlObject(ConfigKey(getGroup(), "osc2_wave"));
    m_pOsc2Wave->setDefaultValue(2.0);
    m_pOsc2Wave->set(2.0);
    m_pOscMix = new ControlPotmeter(ConfigKey(getGroup(), "osc_mix"), 0.0, 1.0);
    m_pOscMix->setDefaultValue(0.5);
    m_pOscMix->set(0.5);
    m_pOsc2Semitones = new ControlPotmeter(ConfigKey(getGroup(), "osc2_semitones"), -24.0, 24.0);
    m_pOsc2Semitones->setDefaultValue(0.0);
    m_pOsc2Semitones->set(0.0);
    m_pOsc2Detune = new ControlPotmeter(ConfigKey(getGroup(), "osc2_detune"), -100.0, 100.0);
    m_pOsc2Detune->setDefaultValue(0.0);
    m_pOsc2Detune->set(7.0);

    // Amplitude envelope, all 0..1 knobs.
    m_pAttack = new ControlPotmeter(ConfigKey(getGroup(), "attack"), 0.0, 1.0);
    m_pAttack->setDefaultValue(0.3);
    m_pAttack->set(0.3);
    m_pDecay = new ControlPotmeter(ConfigKey(getGroup(), "decay"), 0.0, 1.0);
    m_pDecay->setDefaultValue(0.6);
    m_pDecay->set(0.6);
    m_pSustain = new ControlPotmeter(ConfigKey(getGroup(), "sustain"), 0.0, 1.0);
    m_pSustain->setDefaultValue(0.7);
    m_pSustain->set(0.7);
    m_pRelease = new ControlPotmeter(ConfigKey(getGroup(), "release"), 0.0, 1.0);
    m_pRelease->setDefaultValue(0.55);
    m_pRelease->set(0.55);

    // Low-pass filter. cutoff 0..1 is exponential 20 Hz .. 20 kHz; env_amount
    // is how far (in kFilterEnvelopeOctaves) the envelope opens it.
    m_pCutoff = new ControlPotmeter(ConfigKey(getGroup(), "cutoff"), 0.0, 1.0);
    m_pCutoff->setDefaultValue(0.8);
    m_pCutoff->set(0.8);
    m_pResonance = new ControlPotmeter(ConfigKey(getGroup(), "resonance"), 0.0, 1.0);
    m_pResonance->setDefaultValue(0.2);
    m_pResonance->set(0.2);
    m_pEnvAmount = new ControlPotmeter(ConfigKey(getGroup(), "env_amount"), -1.0, 1.0);
    m_pEnvAmount->setDefaultValue(0.0);
    m_pEnvAmount->set(0.3);

    m_pPregain = new ControlAudioTaperPot(ConfigKey(getGroup(), "pregain"), -12, 12, 0.5);

    // Unlike an aux input there is nothing to configure before this channel
    // can make sound, so it goes straight to the main mix; the ON button on
    // the surface toggles main_mix.
    setMainMix(true);
}

EngineSynth::~EngineSynth() {
    delete m_pPregain;
    delete m_pEnvAmount;
    delete m_pResonance;
    delete m_pCutoff;
    delete m_pRelease;
    delete m_pSustain;
    delete m_pDecay;
    delete m_pAttack;
    delete m_pOsc2Detune;
    delete m_pOsc2Semitones;
    delete m_pOscMix;
    delete m_pOsc2Wave;
    delete m_pOsc1Wave;
    delete m_pAllNotesOff;
    delete m_pNoteOff;
    delete m_pNoteOn;
}

void EngineSynth::noteOn(int note, double velocity) {
    if (note < 0 || note >= kNotes) {
        return;
    }
    uint8_t scaled = 127;
    if (velocity > 0.0) {
        scaled = static_cast<uint8_t>(std::clamp(std::lround(velocity * 127.0), 1L, 127L));
    }
    m_velocity[note].store(scaled, std::memory_order_relaxed);
    const uint64_t mask = uint64_t(1) << (note % 64);
    // tapped first: the engine must see the trigger even if the key is
    // released again before the next buffer.
    m_tapped[note / 64].fetch_or(mask, std::memory_order_release);
    m_held[note / 64].fetch_or(mask, std::memory_order_release);
}

void EngineSynth::noteOff(int note) {
    if (note < 0 || note >= kNotes) {
        return;
    }
    const uint64_t mask = uint64_t(1) << (note % 64);
    m_held[note / 64].fetch_and(~mask, std::memory_order_release);
}

void EngineSynth::allNotesOff() {
    for (auto& held : m_held) {
        held.store(0, std::memory_order_release);
    }
    for (auto& tapped : m_tapped) {
        tapped.store(0, std::memory_order_release);
    }
}

bool EngineSynth::isNoteHeld(int note) const {
    if (note < 0 || note >= kNotes) {
        return false;
    }
    const uint64_t mask = uint64_t(1) << (note % 64);
    return (m_held[note / 64].load(std::memory_order_acquire) & mask) != 0;
}

int EngineSynth::activeVoiceCount() const {
    int count = 0;
    for (const Voice& voice : m_voices) {
        if (voice.stage != Stage::Idle) {
            ++count;
        }
    }
    return count;
}

void EngineSynth::slotNoteOn(double v) {
    if (v < 0.0 || v >= kNotes) {
        return;
    }
    const int note = static_cast<int>(std::floor(v));
    const double fraction = v - note;
    // A bare note number is full velocity; note + velocity/128 carries it.
    const double velocity = fraction > 0.0 ? std::min(1.0, fraction * 128.0 / 127.0) : 1.0;
    noteOn(note, velocity);
}

void EngineSynth::slotNoteOff(double v) {
    if (v < 0.0 || v >= kNotes) {
        return;
    }
    noteOff(static_cast<int>(std::floor(v)));
}

void EngineSynth::slotAllNotesOff(double v) {
    if (v > 0.0) {
        allNotesOff();
    }
}

EngineChannel::ActiveState EngineSynth::updateActiveState() {
    const bool keysDown = (m_held[0].load(std::memory_order_acquire) |
                                  m_held[1].load(std::memory_order_acquire) |
                                  m_tapped[0].load(std::memory_order_acquire) |
                                  m_tapped[1].load(std::memory_order_acquire)) != 0;
    if (keysDown || activeVoiceCount() > 0) {
        m_active = true;
        return ActiveState::Active;
    }
    if (m_active) {
        m_vuMeter.reset();
        m_active = false;
        return ActiveState::WasActive;
    }
    return ActiveState::Inactive;
}

void EngineSynth::readParams(Params* pParams) const {
    double sampleRate = m_sampleRate.get();
    if (sampleRate <= 0.0) {
        sampleRate = 44100.0;
    }
    pParams->sampleRate = sampleRate;
    pParams->wave1 = std::clamp(static_cast<int>(std::lround(m_pOsc1Wave->get())), 0, 3);
    pParams->wave2 = std::clamp(static_cast<int>(std::lround(m_pOsc2Wave->get())), 0, 3);
    pParams->oscMix = std::clamp(m_pOscMix->get(), 0.0, 1.0);
    const double semitones = std::round(std::clamp(m_pOsc2Semitones->get(), -24.0, 24.0));
    const double cents = std::clamp(m_pOsc2Detune->get(), -100.0, 100.0);
    pParams->osc2Ratio = std::pow(2.0, (semitones + cents / 100.0) / 12.0);
    pParams->attackInc = 1.0 / (envelopeSeconds(m_pAttack->get(), kMaxAttackSeconds) * sampleRate);
    pParams->decayInc = 1.0 / (envelopeSeconds(m_pDecay->get(), kMaxDecaySeconds) * sampleRate);
    pParams->sustain = std::clamp(m_pSustain->get(), 0.0, 1.0);
    pParams->releaseInc = 1.0 / (envelopeSeconds(m_pRelease->get(), kMaxReleaseSeconds) * sampleRate);
    const double cutoff = std::clamp(m_pCutoff->get(), 0.0, 1.0);
    pParams->cutoffHz = kMinCutoffHz * std::pow(kMaxCutoffHz / kMinCutoffHz, cutoff);
    // damping k = 1/Q: 2.0 (Q 0.5, no peak) down to 0.1 (Q 10).
    pParams->damping = 2.0 * (1.0 - 0.95 * std::clamp(m_pResonance->get(), 0.0, 1.0));
    pParams->envAmountOctaves = std::clamp(m_pEnvAmount->get(), -1.0, 1.0) * kFilterEnvelopeOctaves;
    pParams->gain = m_pPregain->get();
}

EngineSynth::Voice* EngineSynth::findVoiceFor(int note) {
    for (Voice& voice : m_voices) {
        if (voice.stage != Stage::Idle && voice.note == note) {
            return &voice;
        }
    }
    return nullptr;
}

EngineSynth::Voice* EngineSynth::allocateVoice() {
    // Free voice first, then the quietest releasing voice, then the oldest.
    for (Voice& voice : m_voices) {
        if (voice.stage == Stage::Idle) {
            return &voice;
        }
    }
    Voice* pBest = nullptr;
    for (Voice& voice : m_voices) {
        if (voice.stage == Stage::Release && (!pBest || voice.env < pBest->env)) {
            pBest = &voice;
        }
    }
    if (pBest) {
        return pBest;
    }
    for (Voice& voice : m_voices) {
        if (!pBest || voice.startedAt < pBest->startedAt) {
            pBest = &voice;
        }
    }
    return pBest;
}

void EngineSynth::startVoice(int note, double velocity, bool gate) {
    Voice* pVoice = findVoiceFor(note);
    if (!pVoice) {
        pVoice = allocateVoice();
        DEBUG_ASSERT(pVoice);
        if (pVoice->stage == Stage::Idle) {
            pVoice->ic1eq = 0.0;
            pVoice->ic2eq = 0.0;
        }
    }
    // A retriggered voice keeps its envelope level and phases so there is
    // no click; it just climbs again from wherever it is.
    pVoice->note = note;
    pVoice->velocity = velocity;
    pVoice->gate = gate;
    pVoice->stage = Stage::Attack;
    pVoice->startedAt = ++m_voiceSequence;
}

void EngineSynth::releaseVoicesFor(int note) {
    for (Voice& voice : m_voices) {
        if (voice.stage != Stage::Idle && voice.note == note) {
            voice.gate = false;
        }
    }
}

void EngineSynth::applyKeyChanges() {
    for (int block = 0; block < 2; ++block) {
        const uint64_t tapped = m_tapped[block].exchange(0, std::memory_order_acq_rel);
        const uint64_t held = m_held[block].load(std::memory_order_acquire);
        const uint64_t last = m_lastHeld[block];
        m_lastHeld[block] = held;
        const uint64_t starts = tapped | (held & ~last);
        const uint64_t stops = last & ~held;
        if ((starts | stops) == 0) {
            continue;
        }
        for (int bit = 0; bit < 64; ++bit) {
            const uint64_t mask = uint64_t(1) << bit;
            const int note = block * 64 + bit;
            if (stops & mask) {
                releaseVoicesFor(note);
            }
            if (starts & mask) {
                const double velocity = m_velocity[note].load(std::memory_order_relaxed) / 127.0;
                startVoice(note, velocity, (held & mask) != 0);
            }
        }
    }
}

void EngineSynth::renderVoice(Voice* pVoice, const Params& params, CSAMPLE* pMono, std::size_t frames) {
    const double inc1 = noteToHz(pVoice->note) / params.sampleRate;
    const double inc2 = inc1 * params.osc2Ratio;
    const double mix2 = params.oscMix;
    const double mix1 = 1.0 - mix2;
    const double level = pVoice->velocity * kVoiceLevel;
    const double maxCutoff = std::min(kMaxCutoffHz, params.sampleRate * 0.45);
    double a1 = 0.0;
    double a2 = 0.0;
    double a3 = 0.0;
    int untilCoefficients = 0;

    for (std::size_t i = 0; i < frames; ++i) {
        // Envelope. A voice whose key is already up still finishes its
        // attack, so a tap shorter than a buffer is a note rather than a click.
        switch (pVoice->stage) {
        case Stage::Attack:
            pVoice->env += params.attackInc;
            if (pVoice->env >= 1.0) {
                pVoice->env = 1.0;
                pVoice->stage = pVoice->gate ? Stage::Decay : Stage::Release;
            }
            break;
        case Stage::Decay:
            if (!pVoice->gate) {
                pVoice->stage = Stage::Release;
                break;
            }
            pVoice->env -= params.decayInc;
            if (pVoice->env <= params.sustain) {
                pVoice->env = params.sustain;
                pVoice->stage = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            if (!pVoice->gate) {
                pVoice->stage = Stage::Release;
                break;
            }
            pVoice->env = params.sustain;
            break;
        case Stage::Release:
            pVoice->env -= params.releaseInc;
            if (pVoice->env <= 0.0) {
                pVoice->env = 0.0;
                pVoice->stage = Stage::Idle;
                pVoice->note = -1;
                pVoice->gate = false;
                return;
            }
            break;
        case Stage::Idle:
            return;
        }

        if (untilCoefficients == 0) {
            double cutoff = params.cutoffHz * std::pow(2.0, params.envAmountOctaves * pVoice->env);
            cutoff = std::clamp(cutoff, kMinCutoffHz, maxCutoff);
            const double g = std::tan(kPi * cutoff / params.sampleRate);
            a1 = 1.0 / (1.0 + g * (g + params.damping));
            a2 = g * a1;
            a3 = g * a2;
            untilCoefficients = kControlRateSamples;
        }
        --untilCoefficients;

        const double s1 = oscillator(params.wave1, pVoice->phase1, inc1);
        const double s2 = oscillator(params.wave2, pVoice->phase2, inc2);
        pVoice->phase1 += inc1;
        if (pVoice->phase1 >= 1.0) {
            pVoice->phase1 -= 1.0;
        }
        pVoice->phase2 += inc2;
        if (pVoice->phase2 >= 1.0) {
            pVoice->phase2 -= 1.0;
        }
        const double input = s1 * mix1 + s2 * mix2;

        // Topology-preserving state-variable low-pass (Zavalishin), stable
        // up to Nyquist with the cutoff moving under it.
        const double v3 = input - pVoice->ic2eq;
        const double v1 = a1 * pVoice->ic1eq + a2 * v3;
        const double v2 = pVoice->ic2eq + a2 * pVoice->ic1eq + a3 * v3;
        pVoice->ic1eq = 2.0 * v1 - pVoice->ic1eq;
        pVoice->ic2eq = 2.0 * v2 - pVoice->ic2eq;

        pMono[i] += static_cast<CSAMPLE>(v2 * pVoice->env * level);
    }
}

void EngineSynth::process(CSAMPLE* pOut, const std::size_t bufferSize) {
    std::size_t frames = bufferSize / 2;
    VERIFY_OR_DEBUG_ASSERT(frames <= m_monoBuffer.size()) {
        frames = m_monoBuffer.size();
    }

    Params params;
    readParams(&params);
    applyKeyChanges();

    CSAMPLE* pMono = m_monoBuffer.data();
    std::fill_n(pMono, frames, 0.0f);
    bool sounding = false;
    for (Voice& voice : m_voices) {
        if (voice.stage != Stage::Idle) {
            renderVoice(&voice, params, pMono, frames);
            sounding = true;
        }
    }

    if (sounding) {
        const CSAMPLE gain = static_cast<CSAMPLE>(params.gain);
        for (std::size_t i = 0; i < frames; ++i) {
            const CSAMPLE sample = pMono[i] * gain;
            pOut[2 * i] = sample;
            pOut[2 * i + 1] = sample;
        }
        EngineEffectsManager* pEngineEffectsManager = m_pEffectsManager
                ? m_pEffectsManager->getEngineEffectsManager()
                : nullptr;
        if (pEngineEffectsManager != nullptr) {
            pEngineEffectsManager->processPreFaderInPlace(m_group.handle(),
                    m_pEffectsManager->getMainHandle(),
                    pOut,
                    bufferSize,
                    mixxx::audio::SampleRate::fromDouble(params.sampleRate));
        }
    } else {
        SampleUtil::clear(pOut, bufferSize);
    }

    m_vuMeter.process(pOut, bufferSize);
}

void EngineSynth::collectFeatures(GroupFeatureState* pGroupFeatures) const {
    m_vuMeter.collectFeatures(pGroupFeatures);
}
