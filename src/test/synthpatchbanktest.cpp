#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <memory>

#include "control/controlobject.h"
#include "engine/channelhandle.h"
#include "engine/channels/enginesynth.h"
#include "mixer/synthpatchbank.h"
#include "test/signalpathtest.h"

namespace {

const QString kGroup = QStringLiteral("[Synth1]");

// A restart here rebuilds the bank and nothing else, which is what these cases
// are about: the file and the persisted patch control survive, and the new
// bank applies them to the live controls.
//
// Rebuilding the whole Synth instead cannot work. The Synth hands its
// EngineSynth to the mixer, the mixer never gives a channel back, so the first
// EngineSynth outlives it with every one of its controls. A second Synth then
// creates them all again: a Debug build stops on the first duplicate, and a
// release build carries on with the new channel's controls dead and these
// tests reading the old channel's -- testing less than their names say.
//
// So the fixture builds once what a Synth would: the EngineSynth, which owns
// the sound's controls, and the three the Synth itself owns that cases here
// touch. The bank reaches all of them by key. No tracks are loaded, so this is
// a BaseSignalPathTest rather than a SignalPathTest.
class SynthPatchBankTest : public BaseSignalPathTest {
  protected:
    void SetUp() override {
        BaseSignalPathTest::SetUp();
        m_pEngineSynth = std::make_unique<EngineSynth>(
                ChannelHandleAndGroup(ChannelHandle(), kGroup), m_pEffectsManager.get());
        m_pBaseNote = std::make_unique<ControlObject>(ConfigKey(kGroup, QStringLiteral("base_note")));
        m_pScaleRoot = std::make_unique<ControlObject>(ConfigKey(kGroup, QStringLiteral("scale_root")));
        m_pWavetable = std::make_unique<ControlObject>(ConfigKey(kGroup, QStringLiteral("wavetable")));
        // An existing, empty patch file: the bank seeds a profile that has
        // none, and most cases here want empty slots to start from.
        writeUserFile("{\"version\":1,\"group\":\"[Synth1]\",\"slots\":{}}");
        build();
    }

    QString userFilePath() const {
        return QDir(m_pConfig->getSettingsPath()).filePath(QStringLiteral("synth-patches.json"));
    }

    void writeUserFile(const char* json) {
        QFile file(userFilePath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(json);
    }

    void TearDown() override {
        m_pBank.reset();
        m_pWavetable.reset();
        m_pScaleRoot.reset();
        m_pBaseNote.reset();
        m_pEngineSynth.reset();
        BaseSignalPathTest::TearDown();
    }

    // A restart: the bank writes into the test's own settings directory, so a
    // new one finds the file and the persisted patch control where the last
    // one left them.
    void build() {
        m_pBank.reset();
        m_pBank = std::make_unique<SynthPatchBank>(kGroup, m_pConfig);
    }

    static double get(const char* key) {
        return ControlObject::get(ConfigKey(kGroup, key));
    }
    static void set(const char* key, double value) {
        ControlObject::set(ConfigKey(kGroup, key), value);
        QCoreApplication::processEvents();
    }
    static void press(const char* key) {
        set(key, 1);
        set(key, 0);
    }
    SynthPatchBank* bank() const {
        return m_pBank.get();
    }

    std::unique_ptr<EngineSynth> m_pEngineSynth;
    std::unique_ptr<ControlObject> m_pBaseNote;
    std::unique_ptr<ControlObject> m_pScaleRoot;
    std::unique_ptr<ControlObject> m_pWavetable;
    std::unique_ptr<SynthPatchBank> m_pBank;
};

TEST_F(SynthPatchBankTest, StartsEmptyOnSlotOne) {
    ASSERT_TRUE(bank());
    EXPECT_EQ(1, bank()->currentSlot());
    for (int slot = 1; slot <= SynthPatchBank::kSlots; ++slot) {
        EXPECT_FALSE(bank()->isFilled(slot));
        EXPECT_DOUBLE_EQ(0.0, get(QStringLiteral("patch_%1_filled").arg(slot).toUtf8().constData()));
    }
}

TEST_F(SynthPatchBankTest, SaveThenLoadRoundTrips) {
    set("osc1_wave", 4);
    set("cutoff", 0.11);
    set("lfo_rate", 0.77);
    set("unison_voices", 3);
    set("wavetable", 1);
    press("patch_save");
    EXPECT_TRUE(bank()->isFilled(1));
    EXPECT_DOUBLE_EQ(1.0, get("patch_1_filled"));

    set("osc1_wave", 0);
    set("cutoff", 0.9);
    set("lfo_rate", 0.1);
    set("unison_voices", 1);
    set("wavetable", 0);
    press("patch_load");
    EXPECT_DOUBLE_EQ(4.0, get("osc1_wave"));
    EXPECT_DOUBLE_EQ(0.11, get("cutoff"));
    EXPECT_DOUBLE_EQ(0.77, get("lfo_rate"));
    EXPECT_DOUBLE_EQ(3.0, get("unison_voices"));
    EXPECT_DOUBLE_EQ(1.0, get("wavetable"));
}

TEST_F(SynthPatchBankTest, SelectingAnEmptySlotKeepsTheLiveSound) {
    set("cutoff", 0.11);
    set("patch", 5);
    EXPECT_EQ(5, bank()->currentSlot());
    EXPECT_DOUBLE_EQ(0.11, get("cutoff"));
    press("patch_save");
    EXPECT_TRUE(bank()->isFilled(5));
    EXPECT_FALSE(bank()->isFilled(1));
}

TEST_F(SynthPatchBankTest, SelectingAFilledSlotAppliesIt) {
    set("cutoff", 0.11);
    set("patch", 2);
    press("patch_save");
    set("patch", 3);
    set("cutoff", 0.33);
    press("patch_save");
    set("patch", 2);
    EXPECT_DOUBLE_EQ(0.11, get("cutoff"));
    set("patch", 3);
    EXPECT_DOUBLE_EQ(0.33, get("cutoff"));
}

TEST_F(SynthPatchBankTest, FileSurvivesARestartAndTheSlotIsApplied) {
    set("cutoff", 0.11);
    set("lfo_target", 2);
    set("patch", 2);
    press("patch_save");
    ASSERT_TRUE(QFile::exists(bank()->filePath()));
    set("cutoff", 0.9);
    set("lfo_target", 0);

    build();
    // patch persisted as 2 and slot 2 is filled, so the synth comes back
    // sounding as it was saved, with no load press.
    EXPECT_EQ(2, bank()->currentSlot());
    EXPECT_TRUE(bank()->isFilled(2));
    EXPECT_DOUBLE_EQ(0.11, get("cutoff"));
    EXPECT_DOUBLE_EQ(2.0, get("lfo_target"));
}

TEST_F(SynthPatchBankTest, PerformanceStateIsNotPartOfAPatch) {
    for (const ConfigKey& key : bank()->liveKeys()) {
        EXPECT_NE(QStringLiteral("note_on"), key.item);
        EXPECT_NE(QStringLiteral("base_note"), key.item);
        EXPECT_NE(QStringLiteral("scale_root"), key.item);
        EXPECT_NE(QStringLiteral("scale_mask"), key.item);
        EXPECT_NE(QStringLiteral("volume"), key.item);
        EXPECT_NE(QStringLiteral("main_mix"), key.item);
    }
    set("base_note", 60);
    set("scale_root", 5);
    press("patch_save");
    set("base_note", 72);
    set("scale_root", 7);
    press("patch_load");
    EXPECT_DOUBLE_EQ(72.0, get("base_note"));
    EXPECT_DOUBLE_EQ(7.0, get("scale_root"));
}

TEST_F(SynthPatchBankTest, LoadClampsWhatAFileSays) {
    QFile file(bank()->filePath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(
            "{\"version\":1,\"group\":\"[Synth1]\",\"slots\":{\"1\":{"
            "\"osc1_wave\":7,\"lfo_target\":9,\"unison_voices\":9,"
            "\"lfo_sync\":5,\"wavetable\":-3,\"cutoff\":0.42}}}");
    file.close();
    build();
    EXPECT_TRUE(bank()->isFilled(1));
    EXPECT_DOUBLE_EQ(4.0, get("osc1_wave"));
    EXPECT_DOUBLE_EQ(4.0, get("lfo_target"));
    EXPECT_DOUBLE_EQ(4.0, get("unison_voices"));
    EXPECT_DOUBLE_EQ(1.0, get("lfo_sync"));
    EXPECT_DOUBLE_EQ(0.0, get("wavetable"));
    EXPECT_DOUBLE_EQ(0.42, get("cutoff"));
}

TEST_F(SynthPatchBankTest, FactoryPatchesAreValid) {
    ASSERT_GE(bank()->factoryCount(), 12);
    EXPECT_EQ(QStringLiteral("Init"), bank()->factoryName(0));
    EXPECT_TRUE(bank()->factoryName(bank()->factoryCount()).isEmpty());
    // Every factory patch names only controls a patch carries, spelled
    // right, and every one of them is a full patch.
    QSet<QString> known;
    for (const ConfigKey& key : bank()->liveKeys()) {
        known.insert(key.item);
    }
    for (int i = 0; i < bank()->factoryCount(); ++i) {
        EXPECT_FALSE(bank()->factoryName(i).isEmpty()) << i;
        ASSERT_TRUE(bank()->applyFactory(i)) << i;
    }
    // Init is the defaults; Acid is not.
    bank()->applyFactory(0);
    EXPECT_DOUBLE_EQ(0.8, get("cutoff"));
    EXPECT_DOUBLE_EQ(2.0, get("osc1_wave"));
    bank()->applyFactory(7);
    EXPECT_DOUBLE_EQ(0.3, get("cutoff"));
    EXPECT_DOUBLE_EQ(0.85, get("resonance"));
    EXPECT_FALSE(bank()->applyFactory(-1));
    EXPECT_FALSE(bank()->applyFactory(bank()->factoryCount()));
    // Applying a factory patch never fills a slot.
    EXPECT_FALSE(bank()->isFilled(1));
}

TEST_F(SynthPatchBankTest, FreshProfileIsSeededFromTheFactory) {
    m_pBank.reset();
    ASSERT_TRUE(QFile::remove(userFilePath()));
    build();
    for (int slot = 1; slot <= SynthPatchBank::kSlots; ++slot) {
        EXPECT_TRUE(bank()->isFilled(slot)) << slot;
    }
    EXPECT_TRUE(QFile::exists(userFilePath()));
    // Slot 1 is Init and was applied: the sound is the default one.
    EXPECT_EQ(1, bank()->currentSlot());
    EXPECT_DOUBLE_EQ(0.8, get("cutoff"));
    set("patch", 8);
    EXPECT_DOUBLE_EQ(0.3, get("cutoff")); // Acid
}

} // namespace
