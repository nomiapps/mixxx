#include <gtest/gtest.h>

#include <QFile>
#include <memory>

#include "control/controlobject.h"
#include "engine/enginesequencer.h"
#include "engine/sequencerpatternbank.h"
#include "test/mixxxtest.h"

namespace {

const QString kGroup = QStringLiteral("[Sequencer1]");

// The bank against a mixer-less sequencer: its controls are all that is
// needed, and the file goes to the test's temporary directory.
class SequencerPatternBankTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pSeq = std::make_unique<EngineSequencer>(kGroup, nullptr, nullptr);
        m_path = getTestDataDir().filePath(QStringLiteral("sequencer-patterns.json"));
        m_pBank = std::make_unique<SequencerPatternBank>(kGroup, config(), m_path);
    }

    void TearDown() override {
        m_pBank.reset();
        m_pSeq.reset();
    }

    void set(const QString& key, double value) {
        ControlObject::set(ConfigKey(kGroup, key), value);
    }
    double get(const QString& key) {
        return ControlObject::get(ConfigKey(kGroup, key));
    }
    void press(const QString& key) {
        set(key, 1);
        set(key, 0);
    }
    void rebuildBank() {
        m_pBank.reset();
        m_pBank = std::make_unique<SequencerPatternBank>(kGroup, config(), m_path);
    }

    std::unique_ptr<EngineSequencer> m_pSeq;
    std::unique_ptr<SequencerPatternBank> m_pBank;
    QString m_path;
};

TEST_F(SequencerPatternBankTest, SaveThenLoadRoundTrips) {
    set("synth_step_3_note", 60);
    set("sampler_2_step_5_enabled", 1);
    set("length", 8);
    set("swing", 0.3);
    EXPECT_TRUE(m_pBank->saveSlot(1));
    EXPECT_TRUE(m_pBank->isFilled(1));
    EXPECT_EQ(1.0, get("pattern_1_filled"));
    EXPECT_TRUE(QFile::exists(m_path));

    set("synth_step_3_note", 50);
    set("sampler_2_step_5_enabled", 0);
    set("length", 16);
    set("swing", 0);
    EXPECT_TRUE(m_pBank->loadSlot(1));
    EXPECT_EQ(60.0, get("synth_step_3_note"));
    EXPECT_EQ(1.0, get("sampler_2_step_5_enabled"));
    EXPECT_EQ(8.0, get("length"));
    EXPECT_DOUBLE_EQ(0.3, get("swing"));
}

TEST_F(SequencerPatternBankTest, SelectingAnEmptySlotKeepsTheLivePattern) {
    set("synth_step_1_note", 60);
    EXPECT_TRUE(m_pBank->saveSlot(1));
    set("synth_step_1_note", 62);

    set("pattern", 2);
    EXPECT_EQ(2, m_pBank->currentSlot());
    EXPECT_EQ(62.0, get("synth_step_1_note")); // nothing to load, nothing changed
    EXPECT_EQ(0.0, get("pattern_2_filled"));

    press("pattern_save");
    EXPECT_EQ(1.0, get("pattern_2_filled"));

    // Back to slot 1: the filled slot is applied.
    set("pattern", 1);
    EXPECT_EQ(60.0, get("synth_step_1_note"));
    // And forward again: slot 2 holds the edit.
    set("pattern", 2);
    EXPECT_EQ(62.0, get("synth_step_1_note"));
}

TEST_F(SequencerPatternBankTest, LoadButtonRevertsEdits) {
    set("synth_step_4_note", 55);
    EXPECT_TRUE(m_pBank->saveSlot(1));
    set("synth_step_4_note", 70);
    press("pattern_load");
    EXPECT_EQ(55.0, get("synth_step_4_note"));
}

TEST_F(SequencerPatternBankTest, FileSurvivesReconstruction) {
    set("synth_step_2_note", 61);
    EXPECT_TRUE(m_pBank->saveSlot(4));
    rebuildBank();
    EXPECT_TRUE(m_pBank->isFilled(4));
    EXPECT_EQ(1.0, get("pattern_4_filled"));
    set("synth_step_2_note", 40);
    EXPECT_TRUE(m_pBank->loadSlot(4));
    EXPECT_EQ(61.0, get("synth_step_2_note"));
}

TEST_F(SequencerPatternBankTest, MalformedFileLeavesEverySlotEmpty) {
    m_pBank.reset();
    {
        QFile file(m_path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("{not json");
    }
    m_pBank = std::make_unique<SequencerPatternBank>(kGroup, config(), m_path);
    for (int slot = 1; slot <= SequencerPatternBank::kSlots; ++slot) {
        EXPECT_FALSE(m_pBank->isFilled(slot)) << "slot " << slot;
        EXPECT_EQ(0.0, get(QStringLiteral("pattern_%1_filled").arg(slot)));
    }
    // A save rewrites a valid file over the broken one.
    EXPECT_TRUE(m_pBank->saveSlot(3));
    rebuildBank();
    EXPECT_TRUE(m_pBank->isFilled(3));
}

TEST_F(SequencerPatternBankTest, RunAndCurrentStepAreNotPartOfAPattern) {
    set("run", 1);
    EXPECT_TRUE(m_pBank->saveSlot(1));
    set("run", 0);
    EXPECT_TRUE(m_pBank->loadSlot(1));
    EXPECT_EQ(0.0, get("run"));
}

TEST_F(SequencerPatternBankTest, LoadClampsOutOfRangeValues) {
    set("length", 8);
    set("synth_step_1_note", 60);
    EXPECT_TRUE(m_pBank->saveSlot(1));
    // Corrupt the saved file's values by hand, then reload it.
    m_pBank.reset();
    {
        QFile file(m_path);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        QByteArray json = file.readAll();
        file.close();
        json.replace("\"length\": 8", "\"length\": 99");
        json.replace("\"synth_step_1_note\": 60", "\"synth_step_1_note\": 500");
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(json);
    }
    m_pBank = std::make_unique<SequencerPatternBank>(kGroup, config(), m_path);
    EXPECT_TRUE(m_pBank->loadSlot(1));
    EXPECT_EQ(16.0, get("length"));
    EXPECT_EQ(127.0, get("synth_step_1_note"));
}

} // namespace
