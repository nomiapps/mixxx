#include "test/controller_mapping_validation_test.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QApplication>
#include <QRegularExpression>
#include <QUrl>

#include "control/controlobject.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "controllers/scripting/legacy/controllerscriptenginelegacy.h"
#include "effects/effectsmanager.h"
#include "engine/channelhandle.h"
#include "engine/enginemixer.h"
#include "library/coverartcache.h"
#include "library/library.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "track/track.h"
#ifdef MIXXX_USE_QML
#include "qml/qmlplayermanagerproxy.h"
#include "test/newuiqmlqtversion.h"
#endif
#include "moc_controller_mapping_validation_test.cpp"
#include "soundio/soundmanager.h"

namespace {
const QRegularExpression kNonWordPattern(QStringLiteral("[^\\w]+"));
}

FakeMidiControllerJSProxy::FakeMidiControllerJSProxy()
        : ControllerJSProxy(nullptr) {
}

void FakeMidiControllerJSProxy::send(const QList<int>& data, unsigned int length) {
    Q_UNUSED(data);
    Q_UNUSED(length);
    qInfo() << "LINT: Prefer to use sendSysexMsg instead of the generic alias send!";
}

void FakeMidiControllerJSProxy::sendSysexMsg(const QList<int>& data, unsigned int length) {
    Q_UNUSED(data);
    Q_UNUSED(length);
}

void FakeMidiControllerJSProxy::sendShortMsg(unsigned char status,
        unsigned char byte1,
        unsigned char byte2) {
    const char message[] = {static_cast<char>(status),
            static_cast<char>(byte1),
            static_cast<char>(byte2)};
    s_sentShortMessages.append(QByteArray(message, sizeof(message)));
}

QList<QByteArray> FakeMidiControllerJSProxy::s_sentShortMessages;

FakeHidControllerJSProxy::FakeHidControllerJSProxy()
        : ControllerJSProxy(nullptr) {
}

void FakeHidControllerJSProxy::send(const QList<int>& data, unsigned int length) {
    Q_UNUSED(data);
    Q_UNUSED(length);

    qInfo() << "LINT: Prefer to use sendOutputReport instead of send!";
}

void FakeHidControllerJSProxy::send(const QList<int>& dataList,
        unsigned int length,
        quint8 reportID,
        bool useNonSkippingFIFO) {
    Q_UNUSED(dataList);
    Q_UNUSED(length);
    Q_UNUSED(reportID);
    Q_UNUSED(useNonSkippingFIFO);

    qInfo() << "LINT: Prefer to use sendOutputReport instead of send!";
}

void FakeHidControllerJSProxy::sendOutputReport(quint8 reportID,
        const QByteArray& dataArray,
        bool resendUnchangedReport) {
    Q_UNUSED(reportID);
    Q_UNUSED(dataArray);
    Q_UNUSED(resendUnchangedReport);
}

QByteArray FakeHidControllerJSProxy::getInputReport(
        quint8 reportID) {
    Q_UNUSED(reportID);
    return QByteArray();
}

void FakeHidControllerJSProxy::sendFeatureReport(
        quint8 reportID, const QByteArray& reportData) {
    Q_UNUSED(reportID);
    Q_UNUSED(reportData);
}

QByteArray FakeHidControllerJSProxy::getFeatureReport(
        quint8 reportID) {
    Q_UNUSED(reportID);
    return QByteArray();
}

FakeBulkControllerJSProxy::FakeBulkControllerJSProxy()
        : ControllerJSProxy(nullptr) {
}

void FakeBulkControllerJSProxy::send(const QList<int>& data, unsigned int length) {
    Q_UNUSED(data);
    Q_UNUSED(length);
}

FakeController::FakeController()
        : Controller("Test Controller"),
          m_bMidiMapping(false)
#ifdef __HID__
          ,
          m_bHidMapping(false)
#endif
{
    startEngine();
    getScriptEngine()->setTesting(true);
}

FakeController::~FakeController() {
}

bool FakeController::isMappable() const {
    if (m_bMidiMapping) {
        return m_pMidiMapping->isMappable();
    }
#ifdef __HID__
    else if (m_bHidMapping) {
        return m_pHidMapping->isMappable();
    }
#endif
    return false;
}

#ifdef MIXXX_USE_QML
void deleteTrack(Track* pTrack) {
    // Delete track objects directly in unit tests with
    // no main event loop
    delete pTrack;
};
#endif

void MappingTestFixture::SetUp() {
#ifdef MIXXX_USE_QML
    // This setup mirrors coreservices -- it would be nice if we could use coreservices instead
    // but it does a lot of local disk / settings setup.
    auto pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
    m_pEffectsManager = std::make_shared<EffectsManager>(m_pConfig, pChannelHandleFactory);
    m_pEngine = std::make_shared<EngineMixer>(
            m_pConfig,
            "[Master]",
            m_pEffectsManager.get(),
            pChannelHandleFactory,
            true);
    m_pSoundManager = std::make_shared<SoundManager>(m_pConfig, m_pEngine.get());
    m_pControlIndicatorTimer = std::make_shared<mixxx::ControlIndicatorTimer>(nullptr);
    m_pEngine->registerNonEngineChannelSoundIO(gsl::make_not_null(m_pSoundManager.get()));
    m_pPlayerManager = std::make_shared<PlayerManager>(m_pConfig,
            m_pSoundManager.get(),
            m_pEffectsManager.get(),
            m_pEngine.get());

    m_pPlayerManager->addConfiguredDecks();
    m_pPlayerManager->addSampler();
    PlayerInfo::create();
    m_pEffectsManager->setup();

    const auto dbConnection = mixxx::DbConnectionPooled(dbConnectionPooler());
    if (!MixxxDb::initDatabaseSchema(dbConnection)) {
        exit(1);
    }
    m_pTrackCollectionManager = std::make_shared<TrackCollectionManager>(
            nullptr,
            m_pConfig,
            dbConnectionPooler(),
            deleteTrack);

    m_pRecordingManager = std::make_shared<RecordingManager>(m_pConfig, m_pEngine.get());
    CoverArtCache::createInstance();
    m_pLibrary = std::make_shared<Library>(
            nullptr,
            m_pConfig,
            dbConnectionPooler(),
            m_pTrackCollectionManager.get(),
            m_pPlayerManager.get(),
            m_pRecordingManager.get());

    m_pPlayerManager->bindToLibrary(m_pLibrary.get());
    mixxx::qml::QmlPlayerManagerProxy::registerPlayerManager(m_pPlayerManager);
    ControllerScriptEngineBase::registerPlayerManager(m_pPlayerManager);
    ControllerScriptEngineBase::registerTrackCollectionManager(m_pTrackCollectionManager);
#endif
}

void MappingTestFixture::TearDown() {
#ifdef MIXXX_USE_QML
    // Clean up in reverse order of initialization
    PlayerInfo::destroy();
    CoverArtCache::destroy();
    mixxx::qml::QmlPlayerManagerProxy::registerPlayerManager(nullptr);
    ControllerScriptEngineBase::registerPlayerManager(nullptr);
    ControllerScriptEngineBase::registerTrackCollectionManager(nullptr);
#endif
}

bool MappingTestFixture::testLoadMapping(const QString& mappingPath) {
    std::shared_ptr<LegacyControllerMapping> pMapping =
            LegacyControllerMappingFileHandler::loadMapping(
                    QFileInfo(mappingPath), QDir(RESOURCE_FOLDER "/controllers"));
    if (!pMapping) {
        return false;
    }

    FakeController controller;
    controller.setMapping(pMapping);
    bool result = controller.applyMapping(getTestDir().filePath(RESOURCE_FOLDER));
    controller.stopEngine();
    return result;
}

std::unique_ptr<FakeController> MappingTestFixture::startMapping(const QString& mappingPath) {
    const std::shared_ptr<LegacyControllerMapping> pMapping =
            LegacyControllerMappingFileHandler::loadMapping(
                    QFileInfo(mappingPath), QDir(RESOURCE_FOLDER "/controllers"));
    if (!pMapping) {
        return nullptr;
    }
    auto pController = std::make_unique<FakeController>();
    pController->setMapping(pMapping);
    // A controller under test only checks its scripts for syntax errors; this
    // one runs them.
    pController->getScriptEngine()->setTesting(false);
    if (!pController->applyMapping(getTestDir().filePath(RESOURCE_FOLDER))) {
        pController->stopEngine();
        return nullptr;
    }
    return pController;
}

void MappingTestFixture::stopMapping(FakeController* pController) {
    pController->stopEngine();
}

bool checkUrl(const QString& url) {
    return QUrl(url).isValid();
}

bool lintMappingInfo(const MappingInfo& mapping) {
    bool result = true;
    if (mapping.getName().trimmed().isEmpty()) {
        qWarning() << "LINT:" << mapping.getPath() << "has no name.";
        result = false;
    }

    if (mapping.getAuthor().trimmed().isEmpty()) {
        qWarning() << "LINT:" << mapping.getPath() << "has no author.";
    }

    if (mapping.getDescription().trimmed().isEmpty()) {
        qWarning() << "LINT:" << mapping.getPath() << "has no description.";
    }

    if (mapping.getForumLink().trimmed().isEmpty()) {
        qWarning() << "LINT:" << mapping.getPath() << "has no forum link.";
    } else if (!checkUrl(mapping.getForumLink())) {
        qWarning() << "LINT:" << mapping.getPath() << "has invalid forum link";
        result = false;
    }

    if (mapping.getWikiLink().trimmed().isEmpty()) {
        qWarning() << "LINT:" << mapping.getPath() << "has no wiki link.";
    } else if (!checkUrl(mapping.getWikiLink())) {
        qWarning() << "LINT:" << mapping.getPath() << "has invalid wiki link";
        result = false;
    }
    return result;
}

std::string PrintMappingName(const ::testing::TestParamInfo<std::string>& info) {
    auto name = QFileInfo(QString::fromStdString(info.param));
    return name.fileName().replace(kNonWordPattern, "_").toStdString();
}

TEST_P(MappingTestFixture, ValidateMappingXML) {
    QFileInfo mappingPath = QFileInfo(QString::fromStdString(GetParam()));
    qDebug() << "ValidateMappingXML" << mappingPath;

    MappingInfo mapping(mappingPath);
    EXPECT_TRUE(mapping.isValid()) << "Error while validating XML file " << GetParam();
    EXPECT_TRUE(lintMappingInfo(mapping)) << "Error while validating XML file " << GetParam();
}

TEST_P(MappingTestFixture, LoadMapping) {
    QString mappingPath = QString::fromStdString(GetParam());
    qDebug() << "LoadMapping" << mappingPath;

#ifdef MIXXX_USE_QML
    // A mapping that declares a screen renders the New UI's QML onto it, so on
    // an older Qt it fails for a reason that has nothing to do with the mapping.
    // Only those are skipped; every other mapping is still loaded.
    const std::shared_ptr<LegacyControllerMapping> pScreenCheck =
            LegacyControllerMappingFileHandler::loadMapping(
                    QFileInfo(mappingPath), QDir(RESOURCE_FOLDER "/controllers"));
    if (pScreenCheck && !pScreenCheck->getInfoScreens().isEmpty()) {
        SKIP_IF_NEW_UI_QML_UNSUPPORTED();
    }
#endif

    EXPECT_TRUE(testLoadMapping(mappingPath)) << "Error while loading " << GetParam();
}

#ifdef MIXXX_USE_QML
namespace {
const QString kMixtrackPlatinumFxMapping = QStringLiteral(
        RESOURCE_FOLDER "/controllers/Numark Mixtrack Platinum FX.midi.xml");
constexpr int kMixtrackFxUnits = 2;
constexpr int kMixtrackFxButtonsPerUnit = 3;
constexpr char kMixtrackFxButtonLit = 0x7F;
constexpr char kMixtrackFxButtonDim = 0x01;

ConfigKey mixtrackFxEnabledKey(int unit, int button) {
    return ConfigKey(QStringLiteral("[EffectRack1_EffectUnit%1_Effect%2]")
                             .arg(unit + 1)
                             .arg(button + 1),
            QStringLiteral("enabled"));
}

/// The left three buttons are notes 0-2 on channel 9, the right three notes
/// 3-5 on channel 10.
QByteArray mixtrackFxButtonLight(int unit, int button, char brightness) {
    const char message[] = {static_cast<char>(0x98 + unit),
            static_cast<char>(unit * kMixtrackFxButtonsPerUnit + button),
            brightness};
    return QByteArray(message, sizeof(message));
}
} // namespace

/// The controller's six effect buttons and the six effect slots on the screen
/// are the same six things, in the same order, and show the same state.
class MixtrackPlatinumFxMappingTest : public MappingTestFixture {
  protected:
    void loadMapping() {
        m_pController = startMapping(kMixtrackPlatinumFxMapping);
        ASSERT_TRUE(m_pController);
        QCoreApplication::processEvents();
    }

    void TearDown() override {
        if (m_pController) {
            stopMapping(m_pController.get());
            m_pController.reset();
        }
        MappingTestFixture::TearDown();
    }

    std::unique_ptr<FakeController> m_pController;
};

TEST_F(MixtrackPlatinumFxMappingTest, StartsWithNoEffectPickedOnTheScreen) {
    for (int unit = 0; unit < kMixtrackFxUnits; ++unit) {
        for (int button = 0; button < kMixtrackFxButtonsPerUnit; ++button) {
            ControlObject::set(mixtrackFxEnabledKey(unit, button), 1.0);
        }
    }

    loadMapping();

    for (int unit = 0; unit < kMixtrackFxUnits; ++unit) {
        for (int button = 0; button < kMixtrackFxButtonsPerUnit; ++button) {
            EXPECT_EQ(0.0, ControlObject::get(mixtrackFxEnabledKey(unit, button)))
                    << "unit " << unit << " button " << button;
        }
    }
}

TEST_F(MixtrackPlatinumFxMappingTest, LightsTheButtonOfAnEffectSwitchedOnOnTheScreen) {
    loadMapping();

    for (int unit = 0; unit < kMixtrackFxUnits; ++unit) {
        for (int button = 0; button < kMixtrackFxButtonsPerUnit; ++button) {
            FakeMidiControllerJSProxy::s_sentShortMessages.clear();
            ControlObject::set(mixtrackFxEnabledKey(unit, button), 1.0);
            QCoreApplication::processEvents();
            EXPECT_TRUE(FakeMidiControllerJSProxy::s_sentShortMessages.contains(
                    mixtrackFxButtonLight(unit, button, kMixtrackFxButtonLit)))
                    << "unit " << unit << " button " << button;

            FakeMidiControllerJSProxy::s_sentShortMessages.clear();
            ControlObject::set(mixtrackFxEnabledKey(unit, button), 0.0);
            QCoreApplication::processEvents();
            EXPECT_TRUE(FakeMidiControllerJSProxy::s_sentShortMessages.contains(
                    mixtrackFxButtonLight(unit, button, kMixtrackFxButtonDim)))
                    << "unit " << unit << " button " << button;
            EXPECT_FALSE(FakeMidiControllerJSProxy::s_sentShortMessages.contains(
                    mixtrackFxButtonLight(unit, button, kMixtrackFxButtonLit)));
        }
    }
}
#endif

#ifdef __BULK__
INSTANTIATE_TEST_SUITE_P(BulkMappings,
        MappingTestFixture,
        ::testing::Values(CONTROLLER_BULK_MAPPINGS),
        PrintMappingName);
#endif

#ifdef __HID__
INSTANTIATE_TEST_SUITE_P(HidMappings,
        MappingTestFixture,
        ::testing::Values(CONTROLLER_HID_MAPPINGS),
        PrintMappingName);
#endif

INSTANTIATE_TEST_SUITE_P(MidiMappings,
        MappingTestFixture,
        ::testing::Values(CONTROLLER_MIDI_MAPPINGS),
        PrintMappingName);
