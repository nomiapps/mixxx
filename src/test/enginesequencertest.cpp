#include <gtest/gtest.h>

#include <QTest>
#include <cmath>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "engine/channels/enginedeck.h"
#include "engine/channels/enginesynth.h"
#include "engine/enginebuffer.h"
#include "engine/enginesequencer.h"
#include "mixer/sampler.h"
#include "test/signalpathtest.h"
#include "track/beats.h"
#include "track/track.h"
#include "util/defs.h"
#include "util/sample.h"

namespace {

// 126 bpm at 44.1 kHz: 21000 frames per beat, exactly 5250 per 16th step, so
// every expected onset below is an integer frame with no rounding to argue
// about. 2048-frame buffers put those boundaries mid-buffer.
constexpr std::size_t kFrames = 2048;
constexpr double kSampleRate = 44100.0;
constexpr double kBpm = 126.0;
constexpr double kBeatFrames = kSampleRate * 60.0 / kBpm; // 21000
constexpr double kStepFrames = kBeatFrames / 4.0;         // 5250
// An onset detected a sample or two late is the oscillator starting from
// whatever phase the reused voice was left at, not a scheduling error.
constexpr int kOnsetTolerance = 2;

const QString kSynthGroup = QStringLiteral("[Synth1]");
const QString kSeqGroup = QStringLiteral("[Sequencer1]");
const QString kSamplerGroup = QStringLiteral("[Sampler1]");

class EngineSequencerTest : public SignalPathTest {
  protected:
    void SetUp() override {
        m_pOutput = SampleUtil::alloc(kMaxEngineSamples);
        m_pSynth = new EngineSynth(
                ChannelHandleAndGroup(ChannelHandle(), kSynthGroup), m_pEffectsManager.get());
        m_pSeq = new EngineSequencer(kSeqGroup, nullptr, m_pSynth);
        // Instant, plain notes: the shortest attack and release, no filter
        // envelope, and a triangle -- which is not zero at phase 0, unlike
        // the PolyBLEP saw -- so an onset is a silence-to-signal edge and a
        // note-off is a return to exact silence within a few dozen frames.
        setSynth("attack", 0.0);
        setSynth("release", 0.0);
        setSynth("sustain", 1.0);
        setSynth("osc1_wave", 1);
        setSynth("osc_mix", 0.0);
        setSynth("env_amount", 0.0);
    }

    void TearDown() override {
        delete m_pSeq;
        delete m_pSynth;
        SampleUtil::free(m_pOutput);
    }

    void setSynth(const char* key, double value) {
        ControlObject::set(ConfigKey(kSynthGroup, key), value);
    }
    void setSeq(const QString& key, double value) {
        ControlObject::set(ConfigKey(kSeqGroup, key), value);
    }
    double getSeq(const QString& key) {
        return ControlObject::get(ConfigKey(kSeqGroup, key));
    }
    void enableSynthSteps(int count) {
        for (int i = 1; i <= count; ++i) {
            setSeq(QStringLiteral("synth_step_%1_enabled").arg(i), 1);
        }
    }
    void setGate(double gate) {
        for (int i = 1; i <= EngineSequencer::kSteps; ++i) {
            setSeq(QStringLiteral("synth_step_%1_gate").arg(i), gate);
        }
    }

    // One engine callback the way EngineMixer runs it: the sequencer tick,
    // then the synth's state check and process. Records every onset (a
    // non-zero sample after a run of exact silence) as an absolute frame.
    void callback(double phase, double bpm = kBpm) {
        m_pSeq->tick(EngineSequencer::Clock{phase, bpm},
                mixxx::audio::SampleRate::fromDouble(kSampleRate),
                kFrames);
        m_pSynth->updateActiveState();
        m_pSynth->process(m_pOutput, kFrames * 2);
        for (std::size_t i = 0; i < kFrames; ++i) {
            const bool loud = m_pOutput[2 * i] != 0.0f;
            if (loud) {
                if (m_silentRun >= 8) {
                    m_onsets.push_back(m_frame + i);
                }
                m_silentRun = 0;
            } else {
                ++m_silentRun;
            }
        }
        m_frame += kFrames;
    }

    // Free-running clock: the phase advances the way InternalClock does.
    void run(int buffers, double bpm = kBpm) {
        for (int i = 0; i < buffers; ++i) {
            callback(m_phase, bpm);
            m_phase = std::fmod(m_phase + kFrames / (kSampleRate * 60.0 / bpm), 1.0);
        }
    }

    int onsetsBetween(uint64_t from, uint64_t to) const {
        int n = 0;
        for (uint64_t onset : m_onsets) {
            if (onset >= from && onset < to) {
                ++n;
            }
        }
        return n;
    }

    CSAMPLE* m_pOutput;
    EngineSynth* m_pSynth;
    EngineSequencer* m_pSeq;
    std::vector<uint64_t> m_onsets;
    uint64_t m_frame = 0;
    int m_silentRun = 8;
    double m_phase = 0.0;
};

TEST_F(EngineSequencerTest, NothingFiresWhenStopped) {
    enableSynthSteps(4);
    setSeq("run", 0);
    run(20);
    EXPECT_TRUE(m_onsets.empty());
    EXPECT_FALSE(m_pSeq->isRunning());
    EXPECT_EQ(EngineChannel::ActiveState::Inactive, m_pSynth->updateActiveState());
    EXPECT_EQ(-1.0, getSeq("current_step"));
}

TEST_F(EngineSequencerTest, StepOnsetsAtExactFrames) {
    enableSynthSteps(4);
    setGate(0.05);
    setSeq("run", 1);
    run(10);
    ASSERT_GE(m_onsets.size(), 4u);
    // 5250 is 1154 frames into the third buffer: the sub-buffer case.
    EXPECT_NEAR(0.0, m_onsets[0], kOnsetTolerance);
    EXPECT_NEAR(kStepFrames, m_onsets[1], kOnsetTolerance);
    EXPECT_NEAR(2 * kStepFrames, m_onsets[2], kOnsetTolerance);
    EXPECT_NEAR(3 * kStepFrames, m_onsets[3], kOnsetTolerance);
    EXPECT_TRUE(m_pSeq->isRunning());
}

TEST_F(EngineSequencerTest, SwingDelaysOddSteps) {
    enableSynthSteps(4);
    setGate(0.05);
    setSeq("swing", 1.0);
    setSeq("run", 1);
    run(10);
    ASSERT_GE(m_onsets.size(), 4u);
    EXPECT_NEAR(0.0, m_onsets[0], kOnsetTolerance);
    EXPECT_NEAR(1.5 * kStepFrames, m_onsets[1], kOnsetTolerance);
    EXPECT_NEAR(2 * kStepFrames, m_onsets[2], kOnsetTolerance);
    EXPECT_NEAR(3.5 * kStepFrames, m_onsets[3], kOnsetTolerance);
}

TEST_F(EngineSequencerTest, GateSchedulesNoteOffAcrossBuffers) {
    enableSynthSteps(2);
    setGate(0.5); // note-off at 2625, inside the second 2048-frame buffer
    setSeq("run", 1);
    run(1);
    EXPECT_EQ(1, m_pSynth->activeVoiceCount());
    run(1);
    // Released at 2625 and gone a millisecond later, well before this
    // buffer ends at 4096.
    EXPECT_EQ(0, m_pSynth->activeVoiceCount());
    run(2);
    ASSERT_GE(m_onsets.size(), 2u);
    EXPECT_NEAR(kStepFrames, m_onsets[1], kOnsetTolerance);
}

TEST_F(EngineSequencerTest, RunStartsOnNextBeat) {
    enableSynthSteps(16);
    setGate(0.05);
    setSeq("run", 1);
    m_phase = 0.3; // 1.2 steps into the beat: boundaries 2 and 3 are skipped
    run(10);
    ASSERT_GE(m_onsets.size(), 2u);
    EXPECT_NEAR((1.0 - 0.3) * kBeatFrames, m_onsets[0], kOnsetTolerance);
    // Step 1 followed one step later, at 19950, inside these 20480 frames.
    EXPECT_NEAR((1.0 - 0.3) * kBeatFrames + kStepFrames, m_onsets[1], kOnsetTolerance);
    EXPECT_EQ(1.0, getSeq("current_step"));
}

TEST_F(EngineSequencerTest, RestartRealignsAtBeat) {
    enableSynthSteps(16);
    setGate(0.05);
    setSeq("run", 1);
    run(6); // 12288 frames: steps 0, 1, 2 have fired
    EXPECT_EQ(2.0, getSeq("current_step"));
    setSeq("restart", 1);
    setSeq("restart", 0);
    // Step 3 still fires at 15750; the beat at 21000 restarts the pattern.
    run(2); // to 16384
    EXPECT_EQ(3.0, getSeq("current_step"));
    run(3); // to 22528
    EXPECT_EQ(0.0, getSeq("current_step"));
}

TEST_F(EngineSequencerTest, LengthWraps) {
    enableSynthSteps(16);
    setGate(0.05);
    setSeq("length", 4);
    setSeq("run", 1);
    run(12); // 24576 frames: boundaries at 0, 5250, ..., 21000 = five steps
    EXPECT_EQ(0.0, getSeq("current_step"));
    run(3); // 30720: 26250 has fired as well
    EXPECT_EQ(1.0, getSeq("current_step"));
}

TEST_F(EngineSequencerTest, PhaseJumpDoesNotBurst) {
    enableSynthSteps(16);
    setGate(0.05);
    setSeq("run", 1);
    run(2); // phase is now 4096 / 21000 = 0.195
    // A seek on the leader: the phase leaps to 0.7. Without the continuity
    // guard every boundary in between would fire at frame 0 of this buffer.
    const uint64_t jumpStart = m_frame;
    callback(0.7);
    EXPECT_EQ(1, onsetsBetween(jumpStart, jumpStart + kFrames));
}

TEST_F(EngineSequencerTest, StaysActiveWithPendingEvents) {
    enableSynthSteps(1);
    setGate(0.05); // the note-off at 262 lands in this buffer too
    setSeq("run", 1);
    // Tick alone, as EngineMixer does before it asks any channel whether it
    // is active: the scheduled note must already count.
    m_pSeq->tick(EngineSequencer::Clock{0.0, kBpm},
            mixxx::audio::SampleRate::fromDouble(kSampleRate),
            kFrames);
    EXPECT_EQ(2, m_pSynth->scheduledEventCount()); // on, and the gate's off
    EXPECT_EQ(EngineChannel::ActiveState::Active, m_pSynth->updateActiveState());
}

TEST_F(EngineSequencerTest, SamplerLanesDispatchWithoutAMixer) {
    setSeq("sampler_1_step_1_enabled", 1);
    setSeq("sampler_2_step_1_enabled", 1);
    setSeq("run", 1);
    run(1);
    EXPECT_EQ(1, m_pSeq->samplerFireCount(0));
    EXPECT_EQ(1, m_pSeq->samplerFireCount(1));
    EXPECT_EQ(0, m_pSeq->samplerFireCount(2));
}

TEST_F(EngineSequencerTest, SamplerEventCarriesItsOffset) {
    setSeq("sampler_1_step_1_enabled", 1);
    setSeq("run", 1);
    // 130 frames before a beat: the clock primes at ceil(3.975) = 4, a beat
    // boundary, so step 0 fires 130 frames into this buffer.
    callback(1.0 - 130.0 / kBeatFrames);
    EXPECT_EQ(1, m_pSeq->samplerFireCount(0));
    EXPECT_NEAR(130, m_pSeq->lastSamplerFireOffset(0), 1);
}

// The sampler lanes against a real sampler deck in the mixer.
class EngineSequencerSamplerTest : public EngineSequencerTest {
  protected:
    void SetUp() override {
        EngineSequencerTest::SetUp();
        delete m_pSeq;
        m_pSeq = new EngineSequencer(kSeqGroup, m_pEngineMixer.get(), m_pSynth);
        m_pSampler = std::make_unique<Sampler>(nullptr,
                m_pConfig,
                m_pEngineMixer.get(),
                m_pEffectsManager.get(),
                EngineChannel::CENTER,
                m_pEngineMixer->registerChannelGroup(kSamplerGroup));
        // The base fixture builds the decks but loads nothing into them (that
        // is SignalPathTest::SetUp, which this fixture does not run); deck 1
        // gets a track here so it can be the phase-match target below.
        m_pDeckTrack = Track::newTemporary(
                getTestDir().filePath(QStringLiteral("sine-30.wav")));
        loadTrack(m_pMixerDeck1.get(), m_pDeckTrack);
        m_pSamplerTrack = Track::newTemporary(
                getTestDir().filePath(QStringLiteral("sine-30.wav")));
        m_pSampler->slotLoadTrack(m_pSamplerTrack,
#ifdef __STEM__
                mixxx::StemChannelSelection(),
#endif
                false);
        ProcessBuffer();
        EngineBuffer* pBuffer = samplerBuffer();
        for (int i = 0; i < 2000 && !pBuffer->isTrackLoaded(); ++i) {
            QTest::qSleep(1);
        }
        ASSERT_TRUE(pBuffer->isTrackLoaded());
    }

    void TearDown() override {
        m_pSampler.reset();
        EngineSequencerTest::TearDown();
    }

    EngineBuffer* samplerBuffer() {
        return m_pSampler->getEngineDeck()->getEngineBuffer();
    }

    // One mixer callback with the sequencer ticked first, as processChannels
    // would if the mixer owned it.
    void mixerCallback(double phase) {
        m_pSeq->tick(EngineSequencer::Clock{phase, kBpm},
                mixxx::audio::SampleRate::fromDouble(kSampleRate),
                kProcessBufferSize / 2);
        ProcessBuffer();
    }

    // Frames, straight from the engine: the playposition control is a
    // throttled indicator and can still read 0 a buffer after play started.
    double samplerPlayFrames() {
        return samplerBuffer()->getExactPlayPos().value();
    }

    std::unique_ptr<Sampler> m_pSampler;
    TrackPointer m_pSamplerTrack;
    TrackPointer m_pDeckTrack;
};

TEST_F(EngineSequencerSamplerTest, SamplerStartsOnBoundaryBuffer) {
    setSeq("sampler_1_step_1_enabled", 1);
    setSeq("run", 1);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey(kSamplerGroup, "play")));
    mixerCallback(0.0);
    EXPECT_EQ(1, m_pSeq->samplerFireCount(0));
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey(kSamplerGroup, "play")));
    // One buffer into a 30 s file.
    // Started at frame 0 and advanced by at most the one buffer just mixed.
    EXPECT_LT(samplerPlayFrames(), kProcessBufferSize);
}

TEST_F(EngineSequencerSamplerTest, SamplerStartsAtFrameZeroWithQuantizeOn) {
    // The conditions under which a play request queues a phase seek
    // (EngineBuffer::slotControlPlayRequest -> requestSyncPhase -> processSeek):
    // quantize on, which is the engine default, a beatgrid on the sampler's
    // track, and another deck playing to phase-match against. This fixture
    // cannot make that seek actually move the sampler -- beats set after the
    // load do not reach its BpmControl without an event loop -- so the
    // controller path is not contrasted here; the interaction is established
    // from the code, and playFromStartUnquantized() drops the request
    // outright. What is asserted is the sequencer's own path.
    m_pSamplerTrack->trySetBeats(mixxx::Beats::fromConstTempo(
            m_pSamplerTrack->getSampleRate(), mixxx::audio::kStartFramePos, mixxx::Bpm(120)));
    m_pDeckTrack->trySetBeats(mixxx::Beats::fromConstTempo(
            m_pDeckTrack->getSampleRate(), mixxx::audio::kStartFramePos, mixxx::Bpm(120)));
    ControlObject::set(ConfigKey(kSamplerGroup, "quantize"), 1.0);
    ControlObject::set(ConfigKey(m_sGroup1, "play"), 1.0);
    for (int i = 0; i < 10; ++i) {
        ProcessBuffer(); // deck 1 moves off the downbeat
    }

    setSeq("sampler_1_step_1_enabled", 1);
    setSeq("run", 1);
    mixerCallback(0.0);
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey(kSamplerGroup, "play")));
    // Started at frame 0 and advanced by at most the one buffer just mixed.
    EXPECT_LT(samplerPlayFrames(), kProcessBufferSize);
}

TEST_F(EngineSequencerSamplerTest, SamplerStartsOnItsExactFrame) {
    setSeq("sampler_1_step_1_enabled", 1);
    setSeq("run", 1);
    mixerCallback(1.0 - 130.0 / kBeatFrames);
    const int off = m_pSeq->lastSamplerFireOffset(0);
    ASSERT_GT(off, 0);
    // Only the frames after the silent head were rendered, so the playhead
    // sits that far short of a full buffer.
    EXPECT_NEAR(kProcessBufferSize / 2 - off, samplerPlayFrames(), 1.0);
    const auto buffer = m_pEngineMixer->getChannelBuffer(kSamplerGroup);
    // The channel buffer is preallocated at its maximum; only the first
    // kProcessBufferSize samples were written this callback.
    ASSERT_GE(buffer.size(), static_cast<std::size_t>(kProcessBufferSize));
    for (int i = 0; i < 2 * off; ++i) {
        ASSERT_FLOAT_EQ(0.0f, buffer[i]) << "sample " << i;
    }
    // The reader fills its cache asynchronously, so the first buffer after
    // the seek may legitimately be silent past the head as well; that the
    // sampler is really playing is checked over the buffers that follow.
    bool sounded = false;
    for (int b = 0; b < 20 && !sounded; ++b) {
        ProcessBuffer();
        const auto later = m_pEngineMixer->getChannelBuffer(kSamplerGroup);
        for (int i = 0; i < kProcessBufferSize; ++i) {
            if (later[i] != 0.0f) {
                sounded = true;
                break;
            }
        }
    }
    EXPECT_TRUE(sounded);
}

} // namespace
