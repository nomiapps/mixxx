#include <gtest/gtest.h>

#include <cmath>

#include "control/controlobject.h"
#include "engine/channels/enginesynth.h"
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

} // namespace
