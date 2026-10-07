#include "effects/backends/clap/clapbackend.h"

#include <gtest/gtest.h>

#include <QMap>
#include <QSet>

#include "effects/backends/clap/clapeffectprocessor.h"
#include "engine/channelhandle.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/effects/groupfeaturestate.h"
#include "engine/engine.h"
#include "test/mixxxtest.h"
#include "util/samplebuffer.h"

namespace {

// These mirror src/test/clap/testgainplugin.cpp, which CMake builds into
// MIXXX_TEST_CLAP_DIR.
const QString kGainPluginId = QStringLiteral("org.mixxx.test.clap-gain");
const QString kInstrumentPluginId = QStringLiteral("org.mixxx.test.clap-instrument");
const QString kGainParameterId = QStringLiteral("7");
const QString kInvertParameterId = QStringLiteral("9");
constexpr SINT kTestPluginLatencyFrames = 3;

constexpr SINT kFramesPerBuffer = 64;
constexpr CSAMPLE kLeftSample = 0.5f;
constexpr CSAMPLE kRightSample = -0.25f;

class CLAPBackendTest : public MixxxTest {
  protected:
    CLAPBackendTest()
            : m_backend(QStringList{QStringLiteral(MIXXX_TEST_CLAP_DIR)}),
              m_engineParameters(mixxx::audio::SampleRate(44100), kFramesPerBuffer),
              m_input(m_engineParameters.samplesPerBuffer()),
              m_output(m_engineParameters.samplesPerBuffer()) {
        for (SINT i = 0; i < kFramesPerBuffer; ++i) {
            m_input[i * 2] = kLeftSample;
            m_input[i * 2 + 1] = kRightSample;
        }
    }

    /// Loads the test plugin into a processor with one input and one output.
    void createProcessor() {
        m_pManifest = m_backend.getManifest(kGainPluginId);
        ASSERT_TRUE(m_pManifest);
        m_pProcessor = m_backend.createProcessor(m_pManifest);
        ASSERT_TRUE(m_pProcessor);

        for (const auto& pParameter : m_pManifest->parameters()) {
            m_parameters.insert(pParameter->id(),
                    EngineEffectParameterPointer(new EngineEffectParameter(pParameter)));
        }
        m_pProcessor->loadEngineEffectParameters(m_parameters);

        const QString group = QStringLiteral("[Channel1]");
        m_channel = m_handleFactory.getOrCreateHandle(group);
        const QSet<ChannelHandleAndGroup> channels = {ChannelHandleAndGroup(m_channel, group)};
        m_pProcessor->initialize(channels, channels, m_engineParameters);
    }

    void process(EffectEnableState enableState = EffectEnableState::Enabled) {
        m_pProcessor->process(m_channel,
                m_channel,
                m_input.data(),
                m_output.data(),
                m_engineParameters,
                enableState,
                GroupFeatureState());
    }

    void expectOutputScaledBy(CSAMPLE factor) {
        for (SINT i = 0; i < kFramesPerBuffer; ++i) {
            ASSERT_FLOAT_EQ(kLeftSample * factor, m_output[i * 2]);
            ASSERT_FLOAT_EQ(kRightSample * factor, m_output[i * 2 + 1]);
        }
    }

    CLAPBackend m_backend;
    mixxx::EngineParameters m_engineParameters;
    mixxx::SampleBuffer m_input;
    mixxx::SampleBuffer m_output;
    ChannelHandleFactory m_handleFactory;
    ChannelHandle m_channel;
    EffectManifestPointer m_pManifest;
    std::unique_ptr<EffectProcessor> m_pProcessor;
    QMap<QString, EngineEffectParameterPointer> m_parameters;
};

TEST_F(CLAPBackendTest, FindsOnlyAudioEffects) {
    EXPECT_EQ(QList<QString>{kGainPluginId}, m_backend.getEffectIds());
    EXPECT_TRUE(m_backend.canInstantiateEffect(kGainPluginId));
    EXPECT_FALSE(m_backend.canInstantiateEffect(kInstrumentPluginId));
    EXPECT_FALSE(m_backend.getManifest(kInstrumentPluginId));
}

TEST_F(CLAPBackendTest, FolderWithoutPluginsGivesNoEffects) {
    const CLAPBackend backend(QStringList{getTestDataDir().absolutePath()});
    EXPECT_TRUE(backend.getEffectIds().isEmpty());
}

TEST_F(CLAPBackendTest, ManifestDescribesPlugin) {
    const EffectManifestPointer pManifest = m_backend.getManifest(kGainPluginId);
    ASSERT_TRUE(pManifest);
    EXPECT_EQ(EffectBackendType::CLAP, pManifest->backendType());
    EXPECT_EQ(QStringLiteral("Test Gain"), pManifest->name());
    EXPECT_EQ(QStringLiteral("Mixxx"), pManifest->author());

    // The hidden parameter is left out, and so is the plugin's own mix.
    const auto& parameters = pManifest->parameters();
    ASSERT_EQ(2, parameters.size());

    EXPECT_EQ(kGainParameterId, parameters[0]->id());
    EXPECT_EQ(QStringLiteral("Gain"), parameters[0]->name());
    EXPECT_EQ(EffectManifestParameter::ValueScaler::Linear, parameters[0]->valueScaler());
    EXPECT_DOUBLE_EQ(0.0, parameters[0]->getMinimum());
    EXPECT_DOUBLE_EQ(1.0, parameters[0]->getDefault());
    EXPECT_DOUBLE_EQ(2.0, parameters[0]->getMaximum());

    EXPECT_EQ(kInvertParameterId, parameters[1]->id());
    EXPECT_EQ(EffectManifestParameter::ValueScaler::Toggle, parameters[1]->valueScaler());
    const auto& steps = parameters[1]->getSteps();
    ASSERT_EQ(2, steps.size());
    EXPECT_EQ(QStringLiteral("Normal"), steps[0].first);
    EXPECT_EQ(QStringLiteral("Inverted"), steps[1].first);
}

TEST_F(CLAPBackendTest, BackendTypeSurvivesItsName) {
    EXPECT_EQ(EffectBackendType::CLAP,
            EffectsBackend::backendTypeFromString(
                    EffectsBackend::backendTypeToString(EffectBackendType::CLAP)));
}

TEST_F(CLAPBackendTest, ProcessesWithDefaultParameters) {
    ASSERT_NO_FATAL_FAILURE(createProcessor());
    process();
    expectOutputScaledBy(1.0f);
}

TEST_F(CLAPBackendTest, ParameterChangesReachThePlugin) {
    ASSERT_NO_FATAL_FAILURE(createProcessor());
    m_parameters.value(kGainParameterId)->setValue(2.0);
    process();
    expectOutputScaledBy(2.0f);

    m_parameters.value(kInvertParameterId)->setValue(1.0);
    process();
    expectOutputScaledBy(-2.0f);

    // Nothing changed: the plugin keeps what it was last given.
    process(EffectEnableState::Enabling);
    expectOutputScaledBy(-2.0f);
}

TEST_F(CLAPBackendTest, HoldsThePluginsOwnMixFullyWet) {
    ASSERT_NO_FATAL_FAILURE(createProcessor());
    // At the plugin's default mix of 30% this would come out at 1.3.
    m_parameters.value(kGainParameterId)->setValue(2.0);
    process();
    expectOutputScaledBy(2.0f);
}

TEST_F(CLAPBackendTest, ReportsThePluginLatency) {
    ASSERT_NO_FATAL_FAILURE(createProcessor());
    EXPECT_EQ(kTestPluginLatencyFrames, m_pProcessor->getGroupDelayFrames());
}

TEST_F(CLAPBackendTest, PassesThroughAtAnotherSampleRate) {
    ASSERT_NO_FATAL_FAILURE(createProcessor());
    m_parameters.value(kGainParameterId)->setValue(2.0);
    const mixxx::EngineParameters otherRate(mixxx::audio::SampleRate(48000), kFramesPerBuffer);
    m_pProcessor->process(m_channel,
            m_channel,
            m_input.data(),
            m_output.data(),
            otherRate,
            EffectEnableState::Enabled,
            GroupFeatureState());
    expectOutputScaledBy(1.0f);
}

} // anonymous namespace
