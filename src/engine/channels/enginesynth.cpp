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
// Eight full-velocity notes summed at this level stay under 0 dBFS with a
// resonant filter adding a few dB on top; a note's unison voices share
// it, scaled by 1/sqrt(count).
constexpr double kVoiceLevel = 0.2;
// Full unison_detune spreads a note's voices over this many cents.
constexpr double kUnisonMaxCents = 50.0;
// The filter coefficients follow the envelope, but recomputing a tan() for
// every sample of every voice is wasteful; this is inaudible at 44.1 kHz.
constexpr int kControlRateSamples = 32;
constexpr double kMaxAttackSeconds = 4.0;
constexpr double kMaxDecaySeconds = 4.0;
constexpr double kMaxReleaseSeconds = 8.0;
constexpr double kFilterEnvelopeOctaves = 5.0;
// The LFO. Free-running it spans this range exponentially; on cutoff it
// swings this many octaves at full depth; on pitch this many semitones, but
// scaled by depth squared so the bottom of the knob is a vibrato and only
// the top an octave.
constexpr double kLfoMinHz = 0.05;
constexpr double kLfoMaxHz = 20.0;
constexpr double kLfoCutoffOctaves = 4.0;
constexpr double kLfoPitchSemitones = 12.0;
// Synced, lfo_rate picks one of these divisions, in beats: four bars
// down to a thirty-second.
constexpr double kSyncBeats[8] = {16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
constexpr int kSyncDivisions = 8;
// lfo_phase is published to the UI at most this often per cycle.
constexpr double kLfoPhasePublishStep = 1.0 / 64.0;
// The same sanitising as the sequencer's clock.
constexpr double kDefaultBpm = 124.0;
constexpr double kMinBpm = 1.0;
constexpr double kMaxBpm = 400.0;
// Free phase is kept absolute (cycles in the integer part, for sample and
// hold); it wraps here to keep the fraction precise.
constexpr double kLfoPhaseWrap = 1048576.0;

// A small integer hash, so sample and hold is a function of the cycle
// number rather than of any state.
uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

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

// One sample of a wavetable voice: linear interpolation within the frame
// (the guard sample makes index i + 1 valid for every phase below 1) and a
// linear crossfade between the two frames either side of wt_position.
// Interpolating rather than truncating matters: at 2048 points truncation
// leaves a broadband noise floor around -60 dB, audible as hiss on a clean
// sine frame at low notes where many output samples read the same entry;
// the lerp pushes it to around -120 dB for two multiply-adds.
double wavetableSample(const Wavetable& table,
        int frameA,
        int frameB,
        double blend,
        double phase) {
    const double x = phase * kWavetableFrameSize;
    const int i = static_cast<int>(x);
    const double f = x - i;
    const float* a = table.frame(frameA);
    const float* b = table.frame(frameB);
    const double sa = a[i] + (a[i + 1] - a[i]) * f;
    const double sb = b[i] + (b[i + 1] - b[i]) * f;
    return sa + (sb - sa) * blend;
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

// static
double EngineSynth::lfoValue(int shape, double absolutePhase) {
    const double cycle = std::floor(absolutePhase);
    const double phase = absolutePhase - cycle;
    switch (shape) {
    case 1: // triangle, from the top like the oscillator's
        return 4.0 * std::fabs(phase - 0.5) - 1.0;
    case 2: // saw, falling
        return 1.0 - 2.0 * phase;
    case 3: // square
        return phase < 0.5 ? 1.0 : -1.0;
    case 4: { // sample and hold: one value per cycle
        const uint32_t h = hash32(static_cast<uint32_t>(static_cast<int64_t>(cycle)));
        return static_cast<double>(h) / 4294967295.0 * 2.0 - 1.0;
    }
    case 0: // sine
    default:
        return std::sin(kTwoPi * phase);
    }
}

EngineSynth::EngineSynth(const ChannelHandleAndGroup& handleGroup, EffectsManager* pEffectsManager)
        : EngineChannel(handleGroup,
                  EngineChannel::CENTER,
                  pEffectsManager,
                  /*isTalkoverChannel*/ false,
                  /*isPrimaryDeck*/ false),
          m_lastHeld{0, 0},
          m_voiceSequence(0),
          m_leftBuffer(kMaxEngineFrames, 0.0f),
          m_rightBuffer(kMaxEngineFrames, 0.0f),
          m_unisonVoices(1),
          m_scheduledCount(0),
          m_pTable(nullptr),
          m_clockBpm(QStringLiteral("[InternalClock]"),
                  QStringLiteral("bpm"),
                  ControlFlag::AllowMissingOrInvalid),
          m_clockBeatDistance(QStringLiteral("[InternalClock]"),
                  QStringLiteral("beat_distance"),
                  ControlFlag::AllowMissingOrInvalid),
          m_clockPrimed(false),
          m_prevBeatPhase(0.0),
          m_beatCount(0),
          m_beatsAtCallback(0.0),
          m_beatFrames(0.0),
          m_lfoFreePhase(0.0),
          m_lastPublishedPhase(-1.0) {
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

    // Oscillators: 0 sine, 1 triangle, 2 saw, 3 square, 4 wavetable. Plain
    // values rather than push buttons so a surface or a mapping can set them
    // directly.
    m_pOsc1Wave = new ControlObject(ConfigKey(getGroup(), "osc1_wave"));
    m_pOsc1Wave->setDefaultValue(2.0);
    m_pOsc1Wave->set(2.0);
    m_pOsc2Wave = new ControlObject(ConfigKey(getGroup(), "osc2_wave"));
    m_pOsc2Wave->setDefaultValue(2.0);
    m_pOsc2Wave->set(2.0);
    m_pOscMix = new ControlPotmeter(ConfigKey(getGroup(), "osc_mix"), 0.0, 1.0);
    m_pOscMix->setDefaultValue(0.5);
    m_pOscMix->set(0.5);
    // Where in the wavetable wave 4 reads: 0 the first frame, 1 the last.
    m_pWtPosition = new ControlPotmeter(ConfigKey(getGroup(), "wt_position"), 0.0, 1.0);
    m_pWtPosition->setDefaultValue(0.0);
    m_pWtPosition->set(0.0);
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

    // The LFO: shape 0 sine, 1 triangle, 2 saw, 3 square, 4 sample and
    // hold; target 0 off, 1 wavetable position, 2 cutoff, 3 pitch. Depth
    // defaults to 0, so a synth sounds as it did until the knob moves.
    // lfo_rate is a free frequency unless lfo_sync, when it picks a beat
    // division; synced by default, since that is what a DJ wants.
    m_pLfoShape = new ControlObject(ConfigKey(getGroup(), "lfo_shape"));
    m_pLfoShape->setDefaultValue(0.0);
    m_pLfoShape->set(0.0);
    m_pLfoRate = new ControlPotmeter(ConfigKey(getGroup(), "lfo_rate"), 0.0, 1.0);
    m_pLfoRate->setDefaultValue(0.3);
    m_pLfoRate->set(0.3);
    m_pLfoSync = new ControlPushButton(ConfigKey(getGroup(), "lfo_sync"));
    m_pLfoSync->setButtonMode(mixxx::control::ButtonMode::Toggle);
    m_pLfoSync->setDefaultValue(1.0);
    m_pLfoSync->set(1.0);
    m_pLfoDepth = new ControlPotmeter(ConfigKey(getGroup(), "lfo_depth"), 0.0, 1.0);
    m_pLfoDepth->setDefaultValue(0.0);
    m_pLfoDepth->set(0.0);
    m_pLfoTarget = new ControlObject(ConfigKey(getGroup(), "lfo_target"));
    m_pLfoTarget->setDefaultValue(1.0);
    m_pLfoTarget->set(1.0);
    m_pLfoPhase = new ControlObject(ConfigKey(getGroup(), "lfo_phase"));
    m_pLfoPhase->setReadOnly();

    // Unison. One voice by default; detune and spread default to a
    // usable width so turning the count up sounds like unison at once.
    m_pUnisonVoices = new ControlObject(ConfigKey(getGroup(), "unison_voices"));
    m_pUnisonVoices->setDefaultValue(1.0);
    m_pUnisonVoices->set(1.0);
    m_pUnisonDetune = new ControlPotmeter(ConfigKey(getGroup(), "unison_detune"), 0.0, 1.0);
    m_pUnisonDetune->setDefaultValue(0.3);
    m_pUnisonDetune->set(0.3);
    m_pUnisonSpread = new ControlPotmeter(ConfigKey(getGroup(), "unison_spread"), 0.0, 1.0);
    m_pUnisonSpread->setDefaultValue(0.5);
    m_pUnisonSpread->set(0.5);

    // Unlike an aux input there is nothing to configure before this channel
    // can make sound, so it goes straight to the main mix; the ON button on
    // the surface toggles main_mix.
    setMainMix(true);
}

EngineSynth::~EngineSynth() {
    // The callback has stopped, so this is the one place the engine's own
    // table, and anything still queued for it, may be freed.
    delete m_pTable;
    m_pTable = nullptr;
    if (m_wavetablePipe) {
        Wavetable* pQueued = nullptr;
        while (m_wavetablePipe->readMessage(&pQueued)) {
            delete pQueued;
        }
    }
    delete m_pUnisonSpread;
    delete m_pUnisonDetune;
    delete m_pUnisonVoices;
    delete m_pLfoPhase;
    delete m_pLfoTarget;
    delete m_pLfoDepth;
    delete m_pLfoSync;
    delete m_pLfoRate;
    delete m_pLfoShape;
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
    delete m_pWtPosition;
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

void EngineSynth::advanceBeatClock() {
    double bpm = m_clockBpm.get();
    if (!(bpm >= kMinBpm && bpm <= kMaxBpm)) { // also catches NaN
        bpm = kDefaultBpm;
    }
    double phase = m_clockBeatDistance.get();
    if (!std::isfinite(phase)) {
        phase = 0.0;
    }
    phase -= std::floor(phase);
    if (m_clockPrimed && phase < m_prevBeatPhase - 0.5) {
        ++m_beatCount;
    }
    m_clockPrimed = true;
    m_prevBeatPhase = phase;
    m_beatsAtCallback = static_cast<double>(m_beatCount) + phase;
    double sampleRate = m_sampleRate.get();
    if (sampleRate <= 0.0) {
        sampleRate = 44100.0;
    }
    m_beatFrames = sampleRate * 60.0 / bpm;
}

EngineChannel::ActiveState EngineSynth::updateActiveState() {
    // Every callback, whether or not process() follows: a silent synth
    // still has to count bars for the synced LFO.
    advanceBeatClock();
    const bool keysDown = (m_held[0].load(std::memory_order_acquire) |
                                  m_held[1].load(std::memory_order_acquire) |
                                  m_tapped[0].load(std::memory_order_acquire) |
                                  m_tapped[1].load(std::memory_order_acquire)) != 0;
    // Scheduled events count as activity: EngineMixer skips process() for an
    // inactive channel, and that is where they are consumed.
    if (keysDown || activeVoiceCount() > 0 || m_scheduledCount > 0) {
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
    pParams->wave1 = std::clamp(static_cast<int>(std::lround(m_pOsc1Wave->get())), 0, 4);
    pParams->wave2 = std::clamp(static_cast<int>(std::lround(m_pOsc2Wave->get())), 0, 4);
    pParams->pWavetable = m_pTable;
    if (m_pTable != nullptr && m_pTable->frameCount > 0) {
        const double position = std::clamp(m_pWtPosition->get(), 0.0, 1.0) *
                (m_pTable->frameCount - 1);
        pParams->wtFrameA = static_cast<int>(position);
        pParams->wtFrameB = std::min(pParams->wtFrameA + 1, m_pTable->frameCount - 1);
        pParams->wtBlend = position - pParams->wtFrameA;
    } else {
        // No table yet: wave 4 plays the saw rather than nothing.
        pParams->pWavetable = nullptr;
        if (pParams->wave1 == 4) {
            pParams->wave1 = 2;
        }
        if (pParams->wave2 == 4) {
            pParams->wave2 = 2;
        }
    }
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

    pParams->wtPosition = std::clamp(m_pWtPosition->get(), 0.0, 1.0);
    pParams->lfoShape = std::clamp(static_cast<int>(std::lround(m_pLfoShape->get())), 0, 4);
    pParams->lfoTarget = std::clamp(static_cast<int>(std::lround(m_pLfoTarget->get())), 0, 3);
    pParams->lfoDepth = std::clamp(m_pLfoDepth->get(), 0.0, 1.0);
    const double rate = std::clamp(m_pLfoRate->get(), 0.0, 1.0);
    if (m_pLfoSync->toBool() && m_beatFrames > 0.0) {
        // Derived from the clock every buffer, so it cannot drift from it.
        const int division = std::clamp(
                static_cast<int>(std::lround(rate * (kSyncDivisions - 1))), 0, kSyncDivisions - 1);
        const double beats = kSyncBeats[division];
        pParams->lfoPhase0 = m_beatsAtCallback / beats;
        pParams->lfoInc = 1.0 / (beats * m_beatFrames);
    } else {
        const double hz = kLfoMinHz * std::pow(kLfoMaxHz / kLfoMinHz, rate);
        pParams->lfoPhase0 = m_lfoFreePhase;
        pParams->lfoInc = hz / sampleRate;
    }
    pParams->unisonVoices = std::clamp(
            static_cast<int>(std::lround(m_pUnisonVoices->get())), 1, kMaxUnison);
    pParams->unisonCents = std::clamp(m_pUnisonDetune->get(), 0.0, 1.0) * kUnisonMaxCents;
    pParams->unisonSpread = std::clamp(m_pUnisonSpread->get(), 0.0, 1.0);
}

EngineSynth::Voice* EngineSynth::findVoiceFor(int note, int unisonIndex) {
    for (Voice& voice : m_voices) {
        if (voice.stage != Stage::Idle && voice.note == note &&
                voice.unisonIndex == unisonIndex) {
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
    const int count = std::clamp(m_unisonVoices, 1, kMaxUnison);
    for (int i = 0; i < count; ++i) {
        Voice* pVoice = findVoiceFor(note, i);
        if (!pVoice) {
            // Stealing takes any voice, so with everything busy a note's
            // unison can come up thinner than asked. Accepted.
            pVoice = allocateVoice();
            DEBUG_ASSERT(pVoice);
            if (pVoice->stage == Stage::Idle) {
                pVoice->ic1eq = 0.0;
                pVoice->ic2eq = 0.0;
            }
        }
        // A retriggered voice keeps its envelope level and phases so there
        // is no click; it just climbs again from wherever it is.
        pVoice->note = note;
        pVoice->velocity = velocity;
        pVoice->gate = gate;
        pVoice->stage = Stage::Attack;
        pVoice->startedAt = ++m_voiceSequence;
        pVoice->unisonIndex = static_cast<uint8_t>(i);
        pVoice->unisonCount = static_cast<uint8_t>(count);
    }
    // Unison turned down since the note last started: the surplus voices
    // of this note are let go.
    for (Voice& voice : m_voices) {
        if (voice.stage != Stage::Idle && voice.note == note && voice.unisonIndex >= count) {
            voice.gate = false;
        }
    }
}

void EngineSynth::releaseVoicesFor(int note) {
    for (Voice& voice : m_voices) {
        if (voice.stage != Stage::Idle && voice.note == note) {
            voice.gate = false;
        }
    }
}

bool EngineSynth::scheduleNoteOn(std::size_t frameOffset, int note, double velocity) {
    if (note < 0 || note >= kNotes) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_scheduledCount < kMaxScheduledEvents) {
        return false;
    }
    const int scaled = std::clamp(static_cast<int>(std::lround(velocity * 127.0)), 1, 127);
    ScheduledEvent& event = m_scheduled[m_scheduledCount++];
    event.frame = static_cast<uint32_t>(std::min<std::size_t>(frameOffset, kMaxEngineFrames - 1));
    event.note = static_cast<uint8_t>(note);
    event.velocity = static_cast<uint8_t>(scaled);
    event.on = true;
    return true;
}

bool EngineSynth::scheduleNoteOff(std::size_t frameOffset, int note) {
    if (note < 0 || note >= kNotes) {
        return false;
    }
    VERIFY_OR_DEBUG_ASSERT(m_scheduledCount < kMaxScheduledEvents) {
        return false;
    }
    ScheduledEvent& event = m_scheduled[m_scheduledCount++];
    event.frame = static_cast<uint32_t>(std::min<std::size_t>(frameOffset, kMaxEngineFrames - 1));
    event.note = static_cast<uint8_t>(note);
    event.velocity = 0;
    event.on = false;
    return true;
}

int EngineSynth::scheduledEventCount() const {
    return m_scheduledCount;
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

void EngineSynth::renderVoice(Voice* pVoice,
        const Params& params,
        CSAMPLE* pLeft,
        CSAMPLE* pRight,
        std::size_t bufferOffset,
        std::size_t frames) {
    // This voice's place in its note's unison: -1 .. 1 across the voices,
    // 0 for a single one. Detune and pan follow it, from the knobs as they
    // are now, so a held note widens when they turn.
    const int unisonCount = std::max<int>(1, pVoice->unisonCount);
    const double unisonOffset = unisonCount > 1
            ? 2.0 * pVoice->unisonIndex / (unisonCount - 1) - 1.0
            : 0.0;
    const double detuneRatio = std::pow(2.0, unisonOffset * params.unisonCents / 1200.0);
    const double pan = unisonOffset * params.unisonSpread;
    // No-boost pan: a centred voice is 1.0 on both sides, a hard-panned
    // one 1.0 on its side and 0 on the other, so unison at full width does
    // not gain over a single voice the way constant power would.
    const double gainLeft = 1.0 - std::max(0.0, pan);
    const double gainRight = 1.0 + std::min(0.0, pan);
    const double baseInc1 = noteToHz(pVoice->note) * detuneRatio / params.sampleRate;
    // Per control-rate block when the LFO is on pitch.
    double inc1 = baseInc1;
    double inc2 = inc1 * params.osc2Ratio;
    // Per control-rate block when the LFO is on the wavetable position.
    int frameA = params.wtFrameA;
    int frameB = params.wtFrameB;
    double blend = params.wtBlend;
    const double mix2 = params.oscMix;
    const double mix1 = 1.0 - mix2;
    const double level = pVoice->velocity * kVoiceLevel / std::sqrt(static_cast<double>(unisonCount));
    const double maxCutoff = std::min(kMaxCutoffHz, params.sampleRate * 0.45);
    const bool table1 = params.wave1 == 4;
    const bool table2 = params.wave2 == 4;
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
            // The LFO is read on the buffer's timeline, not the segment's,
            // so every voice and every segment agree on it.
            const double lfo = params.lfoDepth > 0.0
                    ? params.lfoDepth *
                            lfoValue(params.lfoShape,
                                    params.lfoPhase0 + (bufferOffset + i) * params.lfoInc)
                    : 0.0;
            double cutoffOctaves = params.envAmountOctaves * pVoice->env;
            if (params.lfoTarget == 2) {
                cutoffOctaves += lfo * kLfoCutoffOctaves;
            }
            if (params.lfoTarget == 3) {
                // depth squared: lfo carries one factor of depth already.
                inc1 = baseInc1 *
                        std::pow(2.0, lfo * params.lfoDepth * kLfoPitchSemitones / 12.0);
                inc2 = inc1 * params.osc2Ratio;
            }
            if (params.lfoTarget == 1 && params.pWavetable != nullptr) {
                const int last = params.pWavetable->frameCount - 1;
                const double position = std::clamp(params.wtPosition + lfo, 0.0, 1.0) * last;
                frameA = static_cast<int>(position);
                frameB = std::min(frameA + 1, last);
                blend = position - frameA;
            }
            double cutoff = params.cutoffHz * std::pow(2.0, cutoffOctaves);
            cutoff = std::clamp(cutoff, kMinCutoffHz, maxCutoff);
            const double g = std::tan(kPi * cutoff / params.sampleRate);
            a1 = 1.0 / (1.0 + g * (g + params.damping));
            a2 = g * a1;
            a3 = g * a2;
            untilCoefficients = kControlRateSamples;
        }
        --untilCoefficients;

        const double s1 = table1
                ? wavetableSample(*params.pWavetable, frameA, frameB, blend, pVoice->phase1)
                : oscillator(params.wave1, pVoice->phase1, inc1);
        const double s2 = table2
                ? wavetableSample(*params.pWavetable, frameA, frameB, blend, pVoice->phase2)
                : oscillator(params.wave2, pVoice->phase2, inc2);
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

        const double sample = v2 * pVoice->env * level;
        pLeft[i] += static_cast<CSAMPLE>(sample * gainLeft);
        pRight[i] += static_cast<CSAMPLE>(sample * gainRight);
    }
}

void EngineSynth::process(CSAMPLE* pOut, const std::size_t bufferSize) {
    std::size_t frames = bufferSize / 2;
    VERIFY_OR_DEBUG_ASSERT(frames <= m_leftBuffer.size()) {
        frames = m_leftBuffer.size();
    }

    drainWavetableLane();
    Params params;
    readParams(&params);
    m_unisonVoices = params.unisonVoices;
    applyKeyChanges();

    // Scheduled events split the buffer into segments: every sounding voice
    // renders up to the next event, the event starts or releases its voice,
    // and rendering continues from there. renderVoice keeps all of a voice's
    // state in the Voice, so a segment is just a pointer and a length.
    for (int i = 1; i < m_scheduledCount; ++i) {
        const ScheduledEvent event = m_scheduled[i];
        int j = i - 1;
        while (j >= 0 && m_scheduled[j].frame > event.frame) {
            m_scheduled[j + 1] = m_scheduled[j];
            --j;
        }
        m_scheduled[j + 1] = event;
    }

    CSAMPLE* pLeft = m_leftBuffer.data();
    CSAMPLE* pRight = m_rightBuffer.data();
    std::fill_n(pLeft, frames, 0.0f);
    std::fill_n(pRight, frames, 0.0f);
    bool sounding = false;
    std::size_t cursor = 0;
    for (int e = 0; e <= m_scheduledCount; ++e) {
        const std::size_t segmentEnd = e < m_scheduledCount
                ? std::min<std::size_t>(m_scheduled[e].frame, frames)
                : frames;
        if (segmentEnd > cursor) {
            for (Voice& voice : m_voices) {
                if (voice.stage != Stage::Idle) {
                    renderVoice(&voice,
                            params,
                            pLeft + cursor,
                            pRight + cursor,
                            cursor,
                            segmentEnd - cursor);
                    sounding = true;
                }
            }
            cursor = segmentEnd;
        }
        if (e < m_scheduledCount) {
            const ScheduledEvent& event = m_scheduled[e];
            if (event.on) {
                startVoice(event.note, event.velocity / 127.0, /*gate*/ true);
            } else {
                releaseVoicesFor(event.note);
            }
            sounding = true;
        }
    }
    m_scheduledCount = 0;

    // Advance the free-running LFO past this buffer, and tell the display
    // where the LFO is, at most every 64th of a cycle.
    if (!m_pLfoSync->toBool()) {
        m_lfoFreePhase += static_cast<double>(frames) * params.lfoInc;
        if (m_lfoFreePhase >= kLfoPhaseWrap) {
            m_lfoFreePhase -= kLfoPhaseWrap;
        }
    }
    const double phaseNow = params.lfoPhase0 - std::floor(params.lfoPhase0);
    if (m_lastPublishedPhase < 0.0 ||
            std::fabs(phaseNow - m_lastPublishedPhase) >= kLfoPhasePublishStep) {
        m_pLfoPhase->setAndConfirm(phaseNow);
        m_lastPublishedPhase = phaseNow;
    }

    if (sounding) {
        const CSAMPLE gain = static_cast<CSAMPLE>(params.gain);
        for (std::size_t i = 0; i < frames; ++i) {
            pOut[2 * i] = pLeft[i] * gain;
            pOut[2 * i + 1] = pRight[i] * gain;
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

void EngineSynth::setWavetablePipe(WavetableEnginePipe&& pipe) {
    m_wavetablePipe.emplace(std::move(pipe));
}

Wavetable* EngineSynth::adoptWavetable(Wavetable* pTable) {
    Wavetable* pPrevious = m_pTable;
    m_pTable = pTable;
    return pPrevious;
}

void EngineSynth::drainWavetableLane() {
    if (!m_wavetablePipe) {
        return;
    }
    Wavetable* pNew = nullptr;
    while (m_wavetablePipe->readMessage(&pNew)) {
        Wavetable* pOld = adoptWavetable(pNew);
        if (pOld != nullptr) {
            // The main side drains its returns before every send, so the
            // return lane never holds more than the send lane can.
            const bool returned = m_wavetablePipe->writeMessage(pOld);
            DEBUG_ASSERT(returned);
            Q_UNUSED(returned);
        }
    }
}

void EngineSynth::collectFeatures(GroupFeatureState* pGroupFeatures) const {
    m_vuMeter.collectFeatures(pGroupFeatures);
}
