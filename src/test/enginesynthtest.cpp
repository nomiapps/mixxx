#include <gtest/gtest.h>

#include <cmath>
#include <functional>

#include "control/controlobject.h"
#include "engine/channels/enginesynth.h"
#include "engine/channels/wavetable.h"
#include "test/signalpathtest.h"
#include "util/defs.h"
#include "util/sample.h"

namespace {

constexpr std::size_t kBufferSize = 2048; // interleaved stereo samples
const QString kGroup = QStringLiteral("[Synth1]");

class EngineSynthTest : public SignalPathTest {
  protected:
    void SetUp() override {
        m_pOutput = SampleUtil::alloc(kMaxEngineSamples);
        // No need for a real handle in this test.
        m_pSynth = new EngineSynth(
                ChannelHandleAndGroup(ChannelHandle(), kGroup), m_pEffectsManager.get());
    }

    void TearDown() override {
        delete m_pSynth;
        SampleUtil::free(m_pOutput);
    }

    // Runs the channel the way EngineMixer does: state update, then process.
    // Returns the peak of the last buffer.
    CSAMPLE render(int buffers) {
        CSAMPLE peak = 0;
        for (int i = 0; i < buffers; ++i) {
            m_pSynth->updateActiveState();
            m_pSynth->process(m_pOutput, kBufferSize);
            peak = 0;
            for (std::size_t s = 0; s < kBufferSize; ++s) {
                peak = std::max(peak, std::fabs(m_pOutput[s]));
            }
        }
        return peak;
    }

    void set(const char* key, double value) {
        ControlObject::set(ConfigKey(kGroup, key), value);
    }

    // A table whose sample i of frame f is fn(f, i), guards filled. The
    // synth takes ownership through adoptWavetable.
    static Wavetable* makeTable(int frames, const std::function<float(int, int)>& fn) {
        Wavetable* pTable = Wavetable::create(frames).release();
        for (int f = 0; f < frames; ++f) {
            for (int i = 0; i < kWavetableFrameSize; ++i) {
                pTable->frame(f)[i] = fn(f, i);
            }
        }
        pTable->fillGuards();
        return pTable;
    }

    static float sine(int i) {
        return static_cast<float>(std::sin(2.0 * M_PI * i / kWavetableFrameSize));
    }

    // Zero crossings of the left channel over the last rendered buffer: a
    // frequency count that needs no FFT.
    int zeroCrossings() const {
        int count = 0;
        for (std::size_t i = 2; i < kBufferSize; i += 2) {
            if ((m_pOutput[i - 2] < 0.0f) != (m_pOutput[i] < 0.0f)) {
                ++count;
            }
        }
        return count;
    }

    // Peak of the left channel per block of frames over the last buffer,
    // as max/min so a level that moves inside the buffer shows as > 1.
    float blockPeakRatio(std::size_t blockFrames) const {
        float lo = 1e9f;
        float hi = 0.0f;
        for (std::size_t start = 0; start + blockFrames <= kBufferSize / 2; start += blockFrames) {
            float peak = 0.0f;
            for (std::size_t f = start; f < start + blockFrames; ++f) {
                peak = std::max(peak, std::fabs(m_pOutput[2 * f]));
            }
            lo = std::min(lo, peak);
            hi = std::max(hi, peak);
        }
        return lo > 0.0f ? hi / lo : 1e9f;
    }

    // Instant envelope, open filter: the output is the oscillator itself.
    void setupClean() {
        set("attack", 0.0);
        set("decay", 0.0);
        set("sustain", 1.0);
        set("release", 0.0);
        set("env_amount", 0.0);
        set("cutoff", 1.0);
        set("resonance", 0.0);
    }

    CSAMPLE* m_pOutput;
    EngineSynth* m_pSynth;
};

TEST_F(EngineSynthTest, SilentAndInactiveWithNoKeys) {
    EXPECT_EQ(EngineChannel::ActiveState::Inactive, m_pSynth->updateActiveState());
    EXPECT_FLOAT_EQ(0.0f, render(2));
    EXPECT_EQ(0, m_pSynth->activeVoiceCount());
}

TEST_F(EngineSynthTest, NoteOnControlStartsAVoice) {
    set("note_on", 60);
    EXPECT_EQ(EngineChannel::ActiveState::Active, m_pSynth->updateActiveState());
    EXPECT_TRUE(m_pSynth->isNoteHeld(60));
    EXPECT_GT(render(4), 0.01f);
    EXPECT_EQ(1, m_pSynth->activeVoiceCount());
    // Stereo: both channels carry the same signal.
    for (std::size_t i = 0; i < kBufferSize; i += 2) {
        ASSERT_FLOAT_EQ(m_pOutput[i], m_pOutput[i + 1]);
    }
}

TEST_F(EngineSynthTest, NoteOffReleasesToSilenceAndGoesInactive) {
    set("note_on", 60);
    render(4);
    set("note_off", 60);
    EXPECT_FALSE(m_pSynth->isNoteHeld(60));
    // Default release is well under a second; 100 buffers of 1024 frames is
    // over two seconds at 44.1 kHz.
    EXPECT_FLOAT_EQ(0.0f, render(100));
    EXPECT_EQ(0, m_pSynth->activeVoiceCount());
    // render() drives updateActiveState() every buffer, so the one-shot
    // WasActive transition has already been consumed by the time we look.
    EXPECT_EQ(EngineChannel::ActiveState::Inactive, m_pSynth->updateActiveState());
}

TEST_F(EngineSynthTest, TapShorterThanABufferStillSounds) {
    m_pSynth->noteOn(64);
    m_pSynth->noteOff(64);
    EXPECT_EQ(EngineChannel::ActiveState::Active, m_pSynth->updateActiveState());
    EXPECT_GT(render(2), 0.001f);
}

TEST_F(EngineSynthTest, SameNoteTwiceRetriggersInsteadOfStacking) {
    set("note_on", 60);
    render(2);
    set("note_on", 60);
    render(2);
    EXPECT_EQ(1, m_pSynth->activeVoiceCount());
}

TEST_F(EngineSynthTest, VelocityScalesLevel) {
    // note + velocity/128: 60.25 is velocity 32 of 127.
    set("note_on", 60.25);
    const CSAMPLE soft = render(8);
    set("all_notes_off", 1);
    set("all_notes_off", 0);
    render(100);
    set("note_on", 60);
    const CSAMPLE loud = render(8);
    EXPECT_GT(soft, 0.0f);
    EXPECT_GT(loud, soft * 2.0f);
}

TEST_F(EngineSynthTest, AllNotesOffReleasesEveryVoice) {
    set("note_on", 60);
    set("note_on", 64);
    set("note_on", 67);
    render(2);
    EXPECT_EQ(3, m_pSynth->activeVoiceCount());
    set("all_notes_off", 1);
    render(100);
    EXPECT_EQ(0, m_pSynth->activeVoiceCount());
}

TEST_F(EngineSynthTest, VoicesAreStolenBeyondPolyphony) {
    for (int note = 48; note < 48 + EngineSynth::kVoices + 4; ++note) {
        m_pSynth->noteOn(note);
    }
    render(1);
    EXPECT_EQ(EngineSynth::kVoices, m_pSynth->activeVoiceCount());
}

TEST_F(EngineSynthTest, OutOfRangeNotesAreIgnored) {
    set("note_on", -1);
    set("note_on", 128);
    m_pSynth->noteOn(200);
    EXPECT_EQ(EngineChannel::ActiveState::Inactive, m_pSynth->updateActiveState());
    EXPECT_FLOAT_EQ(0.0f, render(1));
}

TEST_F(EngineSynthTest, OutputStaysBoundedWithFullPolyphonyAndResonance) {
    set("resonance", 1.0);
    set("cutoff", 0.5);
    set("env_amount", 1.0);
    for (int note = 36; note < 36 + EngineSynth::kVoices; ++note) {
        m_pSynth->noteOn(note);
    }
    for (int i = 0; i < 20; ++i) {
        EXPECT_LT(render(1), 1.5f);
    }
}

TEST_F(EngineSynthTest, ScheduledNoteOnStartsMidBuffer) {
    set("attack", 0.0);
    set("osc1_wave", 1); // triangle: not zero at phase 0
    set("osc_mix", 0.0);
    set("env_amount", 0.0);
    ASSERT_TRUE(m_pSynth->scheduleNoteOn(1000, 60, 1.0));
    EXPECT_EQ(1, m_pSynth->scheduledEventCount());
    // A scheduled event alone keeps the channel out of Inactive.
    EXPECT_EQ(EngineChannel::ActiveState::Active, m_pSynth->updateActiveState());
    m_pSynth->process(m_pOutput, kBufferSize);
    for (std::size_t i = 0; i < 1000; ++i) {
        ASSERT_FLOAT_EQ(0.0f, m_pOutput[2 * i]) << "frame " << i;
    }
    bool sounded = false;
    for (std::size_t i = 1000; i < 1003; ++i) {
        sounded = sounded || m_pOutput[2 * i] != 0.0f;
    }
    EXPECT_TRUE(sounded);
    EXPECT_EQ(0, m_pSynth->scheduledEventCount());
    EXPECT_EQ(1, m_pSynth->activeVoiceCount());
}

TEST_F(EngineSynthTest, ScheduledOffAfterOnInSameBuffer) {
    set("attack", 0.0);
    set("release", 0.0);
    ASSERT_TRUE(m_pSynth->scheduleNoteOn(100, 60, 1.0));
    ASSERT_TRUE(m_pSynth->scheduleNoteOff(300, 60));
    m_pSynth->updateActiveState();
    m_pSynth->process(m_pOutput, kBufferSize);
    // Released at frame 300 and gone within a millisecond, long before
    // the buffer's last frame.
    EXPECT_EQ(0, m_pSynth->activeVoiceCount());
    EXPECT_FLOAT_EQ(0.0f, m_pOutput[2 * (kBufferSize / 2 - 1)]);
}

TEST_F(EngineSynthTest, WavetableBlendsAdjacentFrames) {
    setupClean();
    set("osc1_wave", 4);
    set("osc_mix", 0.0);
    // Frame 0 is a sine, frame 1 its inverse: halfway between them is
    // exactly nothing, either end is the sine.
    EXPECT_EQ(nullptr,
            m_pSynth->adoptWavetable(makeTable(2, [](int f, int i) {
                return f == 0 ? sine(i) : -sine(i);
            })));

    set("wt_position", 0.0);
    set("note_on", 60);
    EXPECT_GT(render(4), 0.01f);
    set("all_notes_off", 1);
    set("all_notes_off", 0);
    render(10);

    set("wt_position", 1.0);
    set("note_on", 60);
    EXPECT_GT(render(4), 0.01f);
    set("all_notes_off", 1);
    set("all_notes_off", 0);
    render(10);

    set("wt_position", 0.5);
    set("note_on", 60);
    EXPECT_LT(render(4), 1e-4f);
    EXPECT_EQ(1, m_pSynth->activeVoiceCount());
}

TEST_F(EngineSynthTest, WavetableOnOsc2) {
    setupClean();
    m_pSynth->adoptWavetable(makeTable(1, [](int, int i) { return sine(i); }));
    set("osc_mix", 1.0);
    set("osc2_wave", 4);
    set("osc2_semitones", 0.0);
    set("osc2_detune", 0.0);
    set("note_on", 60);
    EXPECT_GT(render(4), 0.01f);
}

TEST_F(EngineSynthTest, WavetableWithoutTableFallsBackToSaw) {
    set("osc1_wave", 4);
    set("osc2_wave", 4);
    set("note_on", 60);
    EXPECT_GT(render(4), 0.01f);
}

TEST_F(EngineSynthTest, AdoptWavetableReturnsThePreviousTable) {
    Wavetable* pA = makeTable(1, [](int, int i) { return sine(i); });
    Wavetable* pB = makeTable(1, [](int, int i) { return -sine(i); });
    EXPECT_EQ(nullptr, m_pSynth->adoptWavetable(pA));
    EXPECT_EQ(pA, m_pSynth->wavetable());
    EXPECT_EQ(pA, m_pSynth->adoptWavetable(pB));
    EXPECT_EQ(pB, m_pSynth->wavetable());
    delete pA;
    // pB is the synth's now; its destructor frees it.
}

TEST_F(EngineSynthTest, WavetablePipeDeliversAndReturns) {
    auto pipes = makeTwoWayMessagePipe<Wavetable*, Wavetable*>(
            kWavetableLaneDepth, kWavetableLaneDepth);
    WavetableMainPipe mainSide = std::move(pipes.first);
    m_pSynth->setWavetablePipe(std::move(pipes.second));

    Wavetable* pA = makeTable(1, [](int, int i) { return sine(i); });
    Wavetable* pB = makeTable(1, [](int, int i) { return -sine(i); });
    ASSERT_TRUE(mainSide.writeMessage(pA));
    EXPECT_EQ(nullptr, m_pSynth->wavetable()); // not before the next process()
    render(1);
    EXPECT_EQ(pA, m_pSynth->wavetable());
    Wavetable* pReturned = nullptr;
    EXPECT_FALSE(mainSide.readMessage(&pReturned)); // nothing to give back yet

    ASSERT_TRUE(mainSide.writeMessage(pB));
    render(1);
    EXPECT_EQ(pB, m_pSynth->wavetable());
    ASSERT_TRUE(mainSide.readMessage(&pReturned));
    EXPECT_EQ(pA, pReturned);
    delete pReturned;
}

TEST_F(EngineSynthTest, RetriggerKeepsWavetablePhase) {
    m_pSynth->adoptWavetable(makeTable(1, [](int, int i) { return sine(i); }));
    set("osc1_wave", 4);
    set("note_on", 60);
    render(2);
    set("note_on", 60);
    render(2);
    EXPECT_EQ(1, m_pSynth->activeVoiceCount());
    EXPECT_GT(render(1), 0.01f);
}

TEST_F(EngineSynthTest, WavetableOutputStaysBounded) {
    // A full-scale square is the harshest frame a table can hold.
    m_pSynth->adoptWavetable(makeTable(1, [](int, int i) {
        return i < kWavetableFrameSize / 2 ? 1.0f : -1.0f;
    }));
    set("osc1_wave", 4);
    set("osc2_wave", 4);
    set("resonance", 1.0);
    set("cutoff", 0.5);
    set("env_amount", 1.0);
    for (int note = 36; note < 36 + EngineSynth::kVoices; ++note) {
        m_pSynth->noteOn(note);
    }
    for (int i = 0; i < 20; ++i) {
        EXPECT_LT(render(1), 1.5f);
    }
}

TEST_F(EngineSynthTest, LfoValueShapes) {
    const double phases[4] = {0.0, 0.25, 0.5, 0.75};
    const double sine[4] = {0.0, 1.0, 0.0, -1.0};
    const double triangle[4] = {1.0, 0.0, -1.0, 0.0};
    const double saw[4] = {1.0, 0.5, 0.0, -0.5};
    const double square[4] = {1.0, 1.0, -1.0, -1.0};
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(sine[i], EngineSynth::lfoValue(0, phases[i]), 1e-9) << i;
        EXPECT_NEAR(triangle[i], EngineSynth::lfoValue(1, phases[i]), 1e-9) << i;
        EXPECT_NEAR(saw[i], EngineSynth::lfoValue(2, phases[i]), 1e-9) << i;
        EXPECT_NEAR(square[i], EngineSynth::lfoValue(3, phases[i]), 1e-9) << i;
        // The integer part is the cycle; the shapes repeat.
        EXPECT_NEAR(sine[i], EngineSynth::lfoValue(0, 7.0 + phases[i]), 1e-9) << i;
    }
    // Sample and hold: one value per cycle, the same value on re-evaluation,
    // a different one the next cycle, always in range.
    const double held = EngineSynth::lfoValue(4, 3.1);
    EXPECT_DOUBLE_EQ(held, EngineSynth::lfoValue(4, 3.9));
    EXPECT_NE(held, EngineSynth::lfoValue(4, 4.1));
    EXPECT_GE(held, -1.0);
    EXPECT_LE(held, 1.0);
}

TEST_F(EngineSynthTest, LfoOnCutoffChangesLevelWithinABuffer) {
    // A bright note through a filter the LFO sweeps four octaves either way
    // at 20 Hz: half a sweep fits in one buffer, so the level moves inside
    // it. Without depth it does not.
    setupClean();
    set("cutoff", 0.5);
    set("osc1_wave", 2);
    set("osc_mix", 0.0);
    set("lfo_sync", 0.0);
    set("lfo_rate", 1.0);
    set("lfo_shape", 0);
    set("lfo_target", 2);
    set("lfo_depth", 0.0);
    set("note_on", 96);
    render(3);
    EXPECT_LT(blockPeakRatio(64), 1.15f);
    set("lfo_depth", 1.0);
    render(3);
    EXPECT_GT(blockPeakRatio(64), 1.5f);
}

TEST_F(EngineSynthTest, LfoOnPitchShiftsFrequency) {
    // Synced with a clock that is not moving, the LFO sits at phase 0, where
    // a square is +1: full depth on pitch is then a steady octave up.
    setupClean();
    set("osc1_wave", 0);
    set("osc_mix", 0.0);
    set("lfo_sync", 1.0);
    set("lfo_shape", 3);
    set("lfo_target", 3);
    set("lfo_depth", 0.0);
    set("note_on", 69);
    render(3);
    const int plain = zeroCrossings();
    set("lfo_depth", 1.0);
    render(3);
    const int shifted = zeroCrossings();
    EXPECT_GT(plain, 10);
    EXPECT_NEAR(2.0, static_cast<double>(shifted) / plain, 0.3);
}

TEST_F(EngineSynthTest, LfoSyncFollowsBeatClock) {
    const ConfigKey beatDistance(QStringLiteral("[InternalClock]"), QStringLiteral("beat_distance"));
    set("lfo_sync", 1.0);
    set("lfo_rate", 4.0 / 7.0); // division 4: one beat
    set("note_on", 60);
    ControlObject::set(beatDistance, 0.25);
    render(1);
    EXPECT_NEAR(0.25, ControlObject::get(ConfigKey(kGroup, "lfo_phase")), 0.02);
    ControlObject::set(beatDistance, 0.75);
    render(1);
    EXPECT_NEAR(0.75, ControlObject::get(ConfigKey(kGroup, "lfo_phase")), 0.02);
    // A rollover is a beat; at one bar per cycle the phase is then a
    // quarter plus the fraction.
    ControlObject::set(beatDistance, 0.9);
    render(1);
    ControlObject::set(beatDistance, 0.1);
    set("lfo_rate", 2.0 / 7.0); // division 2: one bar
    render(1);
    EXPECT_NEAR(1.1 / 4.0, ControlObject::get(ConfigKey(kGroup, "lfo_phase")), 0.02);
    ControlObject::set(beatDistance, 0.0);
}

} // namespace
