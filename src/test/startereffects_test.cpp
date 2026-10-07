#include <gtest/gtest.h>

#include <memory>

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

    std::shared_ptr<EffectsManager> m_pEffectsManager;
};

TEST_F(StarterEffectsTest, FillsTheFirstTwoUnitsOfANewProfile) {
    m_pEffectsManager->loadStarterEffects();

    EXPECT_EQ(FilterEffect::getId(), effectId(0, 0));
    EXPECT_EQ(FilterEffect::getId(), effectId(0, 1));
    EXPECT_EQ(FlangerEffect::getId(), effectId(0, 2));
    // One high-pass, one low-pass: the Filter turns at the centre of its knob.
    EXPECT_GT(slot(0, 0)->getMetaParameter(), 0.5);
    EXPECT_LT(slot(0, 1)->getMetaParameter(), 0.5);

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
