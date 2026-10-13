#include <gtest/gtest.h>

#include <memory>

#include "control/controlobject.h"
#include "effects/backends/builtin/echoeffect.h"
#include "effects/backends/builtin/filtereffect.h"
#include "effects/backends/builtin/flangereffect.h"
#include "effects/backends/builtin/phasereffect.h"
#include "effects/effectchain.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "effects/presets/effectchainpreset.h"
#include "engine/channelhandle.h"
#include "test/mixxxtest.h"

namespace {

constexpr int kStarterUnits = 2;
constexpr int kStarterEffectsPerUnit = 3;

class StarterEffectsTest : public MixxxTest {
  protected:
    void SetUp() override {
        auto pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager =
                std::make_shared<EffectsManager>(config(), pChannelHandleFactory);
        const QString mainOutputGroup = QStringLiteral("[MasterOutput]");
        m_pEffectsManager->registerInputChannel(ChannelHandleAndGroup(
                pChannelHandleFactory->getOrCreateHandle(mainOutputGroup), mainOutputGroup));
        m_pEffectsManager->setup();
    }

    void TearDown() override {
        clearUnits();
        m_pEffectsManager.reset();
    }

    void clearUnits() {
        for (int unit = 0; unit < kNumStandardEffectUnits; ++unit) {
            auto pEmptyPreset = EffectChainPresetPointer::create();
            pEmptyPreset->setName(QString());
            m_pEffectsManager->getStandardEffectChain(unit)->loadChainPreset(pEmptyPreset);
        }
    }

    EffectSlotPointer slot(int unit, int slotNumber) {
        return m_pEffectsManager->getStandardEffectChain(unit)->getEffectSlot(slotNumber);
    }

    QString effectId(int unit, int slotNumber) {
        const EffectManifestPointer pManifest = slot(unit, slotNumber)->getManifest();
        return pManifest ? pManifest->id() : QString();
    }

    /// The Filter's knobs are low-pass (1), resonance (2), high-pass (3).
    double filterKnob(int slotNumber, int knob) {
        return ControlObject::get(ConfigKey(slot(0, slotNumber)->getGroup(),
                QStringLiteral("parameter%1").arg(knob)));
    }

    /// Slot 1 cuts the lows, slot 2 the highs, and neither follows the meta knob.
    void expectHighPassAndLowPass() {
        constexpr double kFilterOpenLowPassHz = 22050;
        constexpr double kFilterOpenHighPassHz = 13;
        EXPECT_DOUBLE_EQ(500.0, filterKnob(0, 3));
        EXPECT_DOUBLE_EQ(kFilterOpenLowPassHz, filterKnob(0, 1));
        EXPECT_DOUBLE_EQ(1000.0, filterKnob(1, 1));
        EXPECT_DOUBLE_EQ(kFilterOpenHighPassHz, filterKnob(1, 3));
    }

    double knob(int unit, int slotNumber, int knobNumber) {
        return ControlObject::get(ConfigKey(slot(unit, slotNumber)->getGroup(),
                QStringLiteral("parameter%1").arg(knobNumber)));
    }

    /// The Flanger is fully mixed in (its fifth knob) and the Phaser deep
    /// (fifth) and fed back (second): both can be heard.
    void expectFlangerAndPhaserCanBeHeard() {
        EXPECT_DOUBLE_EQ(1.0, knob(0, 2, 5));
        EXPECT_DOUBLE_EQ(1.0, knob(1, 2, 5));
        EXPECT_DOUBLE_EQ(0.5, knob(1, 2, 2));
    }

    void markProfileAsHavingVersion(int version) {
        config()->setValue(ConfigKey(QStringLiteral("[Effects]"),
                                   QStringLiteral("StarterEffectsVersion")),
                version);
    }

    EffectManifestPointer builtIn(const QString& id) {
        return m_pEffectsManager->getBackendManager()->getManifest(
                id, EffectBackendType::BuiltIn);
    }

    std::shared_ptr<EffectsManager> m_pEffectsManager;
};

TEST_F(StarterEffectsTest, FillsTheFirstTwoUnitsOfANewProfile) {
    m_pEffectsManager->loadStarterEffects();

    EXPECT_EQ(FilterEffect::getId(), effectId(0, 0));
    EXPECT_EQ(FilterEffect::getId(), effectId(0, 1));
    EXPECT_EQ(FlangerEffect::getId(), effectId(0, 2));
    expectHighPassAndLowPass();
    expectFlangerAndPhaserCanBeHeard();

    // A delay and a reverb: the built-in ones, or plugins where this machine
    // has them installed.
    for (int slotNumber = 0; slotNumber < kStarterEffectsPerUnit; ++slotNumber) {
        EXPECT_TRUE(slot(1, slotNumber)->isLoaded());
    }
    EXPECT_EQ(PhaserEffect::getId(), effectId(1, 2));

    // The other units stay empty.
    for (int unit = kStarterUnits; unit < kNumStandardEffectUnits; ++unit) {
        EXPECT_TRUE(m_pEffectsManager->getStandardEffectChain(unit)->isEmpty());
    }
}

/// The unit's super knob drives every effect's meta knob, and is put back to
/// its centre whenever the unit is loaded. A filter set through its meta knob
/// went back to doing nothing then; these hold their cutoff themselves.
TEST_F(StarterEffectsTest, FiltersKeepTheirCutoffWhenTheSuperKnobMoves) {
    m_pEffectsManager->loadStarterEffects();

    const auto pChain = m_pEffectsManager->getStandardEffectChain(0);
    for (const double super : {0.0, 1.0, 0.5}) {
        ControlObject::set(ConfigKey(pChain->group(), QStringLiteral("super1")), super);
        expectHighPassAndLowPass();
    }
}

/// The Flanger's mix and the Phaser's depth follow the meta knob by default,
/// and a meta knob at zero silences the one and flattens the other.
TEST_F(StarterEffectsTest, FlangerAndPhaserKeepTheirStrengthWhateverTheMetaKnobDoes) {
    m_pEffectsManager->loadStarterEffects();

    for (const double meta : {0.0, 1.0, 0.5}) {
        ControlObject::set(ConfigKey(slot(0, 2)->getGroup(), QStringLiteral("meta")), meta);
        ControlObject::set(ConfigKey(slot(1, 2)->getGroup(), QStringLiteral("meta")), meta);
        expectFlangerAndPhaserCanBeHeard();
    }
}

/// A profile that has the second version of the starter effects has a Flanger
/// mixed in at nothing. It and the Phaser are put right once; the Filters,
/// which the second version already put right, are not touched again.
TEST_F(StarterEffectsTest, RepairsTheFlangerAndPhaserOfTheSecondVersion) {
    ASSERT_TRUE(builtIn(FlangerEffect::getId()));
    ASSERT_TRUE(builtIn(PhaserEffect::getId()));
    slot(0, 2)->loadEffectWithDefaults(builtIn(FlangerEffect::getId()));
    slot(1, 2)->loadEffectWithDefaults(builtIn(PhaserEffect::getId()));
    ControlObject::set(ConfigKey(slot(0, 2)->getGroup(), QStringLiteral("meta")), 0.0);
    ControlObject::set(ConfigKey(slot(1, 2)->getGroup(), QStringLiteral("meta")), 0.0);
    ASSERT_DOUBLE_EQ(0.0, knob(0, 2, 5)) << "the fault this repairs: a silent Flanger";
    // A filter the second version put right, and the user then moved.
    slot(0, 0)->loadEffectWithDefaults(builtIn(FilterEffect::getId()));
    constexpr double kLinkTypeNone = 0;
    constexpr double kUsersHighPassHz = 800.0;
    ControlObject::set(ConfigKey(slot(0, 0)->getGroup(),
                               QStringLiteral("parameter3_link_type")),
            kLinkTypeNone);
    ControlObject::set(ConfigKey(slot(0, 0)->getGroup(), QStringLiteral("parameter3")),
            kUsersHighPassHz);
    markProfileAsHavingVersion(2);

    m_pEffectsManager->loadStarterEffects();

    expectFlangerAndPhaserCanBeHeard();
    EXPECT_DOUBLE_EQ(kUsersHighPassHz, filterKnob(0, 3));
}

/// A profile that was given the first starter effects has two Filters that do
/// nothing. They are put right once, if they still follow the meta knob.
TEST_F(StarterEffectsTest, RepairsTheFiltersOfTheFirstVersion) {
    const EffectManifestPointer pFilter =
            m_pEffectsManager->getBackendManager()->getManifest(
                    FilterEffect::getId(), EffectBackendType::BuiltIn);
    ASSERT_TRUE(pFilter);
    slot(0, 0)->loadEffectWithDefaults(pFilter);
    slot(0, 1)->loadEffectWithDefaults(pFilter);
    config()->setValue(ConfigKey(QStringLiteral("[Effects]"),
                               QStringLiteral("StarterEffectsLoaded")),
            true);

    m_pEffectsManager->loadStarterEffects();

    expectHighPassAndLowPass();
    // Only the filters: nothing else is loaded into a profile that was asked before.
    EXPECT_FALSE(slot(0, 2)->isLoaded());
    EXPECT_TRUE(m_pEffectsManager->getStandardEffectChain(1)->isEmpty());
}

TEST_F(StarterEffectsTest, LeavesAFilterTheUserHasSetAlone) {
    const EffectManifestPointer pFilter =
            m_pEffectsManager->getBackendManager()->getManifest(
                    FilterEffect::getId(), EffectBackendType::BuiltIn);
    ASSERT_TRUE(pFilter);
    slot(0, 0)->loadEffectWithDefaults(pFilter);
    // Taken off the meta knob and set by hand.
    constexpr double kUsersHighPassHz = 250.0;
    constexpr double kLinkTypeNone = 0;
    ControlObject::set(ConfigKey(slot(0, 0)->getGroup(),
                               QStringLiteral("parameter3_link_type")),
            kLinkTypeNone);
    ControlObject::set(ConfigKey(slot(0, 0)->getGroup(), QStringLiteral("parameter3")),
            kUsersHighPassHz);
    config()->setValue(ConfigKey(QStringLiteral("[Effects]"),
                               QStringLiteral("StarterEffectsLoaded")),
            true);

    m_pEffectsManager->loadStarterEffects();

    EXPECT_DOUBLE_EQ(kUsersHighPassHz, filterKnob(0, 3));
}

TEST_F(StarterEffectsTest, AsksAProfileOnlyOnce) {
    m_pEffectsManager->loadStarterEffects();
    clearUnits();

    m_pEffectsManager->loadStarterEffects();

    EXPECT_TRUE(m_pEffectsManager->getStandardEffectChain(0)->isEmpty());
    EXPECT_TRUE(m_pEffectsManager->getStandardEffectChain(1)->isEmpty());
}

TEST_F(StarterEffectsTest, LeavesUnitsTheUserHasFilledAlone) {
    const EffectManifestPointer pEcho =
            m_pEffectsManager->getBackendManager()->getManifest(
                    EchoEffect::getId(), EffectBackendType::BuiltIn);
    ASSERT_TRUE(pEcho);
    slot(1, 3)->loadEffectWithDefaults(pEcho);

    m_pEffectsManager->loadStarterEffects();

    EXPECT_TRUE(m_pEffectsManager->getStandardEffectChain(0)->isEmpty());
    EXPECT_FALSE(slot(1, 0)->isLoaded());
    EXPECT_EQ(EchoEffect::getId(), effectId(1, 3));
}

} // anonymous namespace
