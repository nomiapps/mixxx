#include <gtest/gtest.h>

#include <QCoreApplication>
#include <memory>

#include "control/controlobject.h"
#include "mixer/synth.h"
#include "mixer/wavetablelibrary.h"
#include "test/signalpathtest.h"

namespace {

const QString kGroup = QStringLiteral("[Synth1]");

class SynthTest : public SignalPathTest {
  protected:
    void SetUp() override {
        m_pSynth = std::make_unique<Synth>(nullptr,
                kGroup,
                m_pConfig,
                m_pEngineMixer.get(),
                m_pEffectsManager.get());
    }

    void TearDown() override {
        m_pSynth.reset();
    }

    static double get(const char* key) {
        return ControlObject::get(ConfigKey(kGroup, key));
    }
    static void set(const char* key, double value) {
        ControlObject::set(ConfigKey(kGroup, key), value);
    }

    std::unique_ptr<Synth> m_pSynth;
};

TEST_F(SynthTest, StartsWithTheFirstBuiltinLoaded) {
    EXPECT_EQ(WavetableLibrary::kBuiltinCount, m_pSynth->wavetableCount());
    EXPECT_EQ(0, m_pSynth->currentWavetable());
    EXPECT_DOUBLE_EQ(wavetable::kBuiltinFrames, get("wt_frames"));
    ASSERT_TRUE(m_pSynth->currentTable());
    EXPECT_EQ(wavetable::kBuiltinFrames, m_pSynth->currentTable()->frameCount);
}

TEST_F(SynthTest, TheControlSelectsATable) {
    const auto pBefore = m_pSynth->currentTable();
    int changes = 0;
    QObject::connect(m_pSynth.get(), &Synth::wavetableChanged, [&changes] { ++changes; });
    set("wavetable", 1);
    QCoreApplication::processEvents();
    EXPECT_EQ(1, m_pSynth->currentWavetable());
    EXPECT_EQ(1, changes);
    EXPECT_DOUBLE_EQ(wavetable::kBuiltinFrames, get("wt_frames"));
    EXPECT_NE(pBefore, m_pSynth->currentTable());
}

TEST_F(SynthTest, OutOfRangeSelectionsClamp) {
    set("wavetable", 99);
    QCoreApplication::processEvents();
    EXPECT_EQ(WavetableLibrary::kBuiltinCount - 1, m_pSynth->currentWavetable());
    EXPECT_DOUBLE_EQ(WavetableLibrary::kBuiltinCount - 1, get("wavetable"));
    m_pSynth->selectWavetable(-5);
    QCoreApplication::processEvents();
    EXPECT_EQ(0, m_pSynth->currentWavetable());
    EXPECT_DOUBLE_EQ(0.0, get("wavetable"));
}

TEST_F(SynthTest, FramesAreReadOnly) {
    set("wt_frames", 3);
    EXPECT_DOUBLE_EQ(wavetable::kBuiltinFrames, get("wt_frames"));
}

} // namespace
