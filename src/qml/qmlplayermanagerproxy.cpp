#include "qml/qmlplayermanagerproxy.h"

#include <QQmlEngine>

#include "control/controlobject.h"
#include "library/library_prefs.h"
#include "mixer/playermanager.h"
#include "mixer/sampler.h"
#include "moc_qmlplayermanagerproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "qml/qmlplayerproxy.h"
#include "sources/soundsourceproxy.h"
#include "track/track_decl.h"
#include "util/fileinfo.h"
#include "util/sandbox.h"

namespace mixxx {
namespace qml {

QmlPlayerManagerProxy::QmlPlayerManagerProxy(
        std::shared_ptr<PlayerManager> pPlayerManager, QObject* parent)
        : QObject(parent),
          m_pPlayerManager(pPlayerManager) {
}

QmlPlayerProxy* QmlPlayerManagerProxy::getPlayer(const QString& group) {
    BaseTrackPlayer* pPlayer = m_pPlayerManager->getPlayer(group);
    if (!pPlayer) {
        qWarning() << "PlayerManagerProxy failed to find player for group" << group;
        return nullptr;
    }

    // Don't set a parent here, so that the QML engine deletes the object when
    // the corresponding JS object is garbage collected.
    QmlPlayerProxy* pPlayerProxy = new QmlPlayerProxy(pPlayer);
    QQmlEngine::setObjectOwnership(pPlayerProxy, QQmlEngine::JavaScriptOwnership);
    connect(pPlayerProxy,
            &QmlPlayerProxy::loadTrackFromLocationRequested,
            this,
            [this, group](const QString& trackLocation, bool play) {
                loadLocationToPlayer(trackLocation, group, play);
            });
    connect(pPlayerProxy,
            &QmlPlayerProxy::loadTrackRequested,
            this,
            [this, group](TrackPointer track,
#ifdef __STEM__
                    mixxx::StemChannelSelection stemSelection,
#endif
                    bool play) {
                loadTrackToPlayer(track, group,
#ifdef __STEM__
                        stemSelection,
#endif
                        play);
            });
    connect(pPlayerProxy,
            &QmlPlayerProxy::cloneFromGroup,
            this,
            [this, group](const QString& sourceGroup) {
                m_pPlayerManager->slotCloneDeck(sourceGroup, group);
            });
    return pPlayerProxy;
}

void QmlPlayerManagerProxy::loadLocationIntoNextAvailableDeck(
        const QString& trackLocation, bool play) {
    m_pPlayerManager->slotLoadLocationIntoNextAvailableDeck(trackLocation, play);
}

void QmlPlayerManagerProxy::loadLocationUrlIntoNextAvailableDeck(
        const QUrl& trackLocationUrl, bool play) {
    if (trackLocationUrl.isLocalFile()) {
        loadLocationIntoNextAvailableDeck(trackLocationUrl.toLocalFile(), play);
    } else {
        qWarning() << "QmlPlayerManagerProxy: URL" << trackLocationUrl << "is not a local file!";
    }
}

void QmlPlayerManagerProxy::loadLocationUrlToDeck(
        const QUrl& locationUrl, int deck) {
    if (!locationUrl.isLocalFile()) {
        qWarning() << "QmlPlayerManagerProxy: URL" << locationUrl
                   << "is not a local file!";
        return;
    }
    mixxx::FileInfo fileInfo(locationUrl.toLocalFile());
    Sandbox::createSecurityToken(&fileInfo);
    m_pPlayerManager->slotLoadToDeck(fileInfo.location(), deck);
}

void QmlPlayerManagerProxy::loadLocationToPlayer(
        const QString& location, const QString& group, bool play) {
    m_pPlayerManager->slotLoadLocationToPlayer(location, group, play);
}

void QmlPlayerManagerProxy::loadTrackToPlayer(TrackPointer track,
        const QString& group,
#ifdef __STEM__
        mixxx::StemChannelSelection stemSelection,
#endif
        bool play) {
    m_pPlayerManager->slotLoadTrackToPlayer(track, group,
#ifdef __STEM__
            stemSelection,
#endif
            play);
}

namespace {
void applyLoopToSampler(const QString& destGroup, double loopStart, double loopEnd) {
    ControlObject::set(ConfigKey(destGroup, QStringLiteral("loop_start_position")), loopStart);
    ControlObject::set(ConfigKey(destGroup, QStringLiteral("loop_end_position")), loopEnd);
    ControlObject::set(ConfigKey(destGroup, QStringLiteral("cue_point")), loopStart);
    // Reloop toggle (1 then 0) enables the loop without sticking the button.
    ControlObject::set(ConfigKey(destGroup, QStringLiteral("reloop_toggle")), 1.0);
    ControlObject::set(ConfigKey(destGroup, QStringLiteral("reloop_toggle")), 0.0);
    ControlObject::set(ConfigKey(destGroup, QStringLiteral("play")), 0.0);
}
} // namespace

int QmlPlayerManagerProxy::sampleLoopToSampler(
        const QString& sourceGroup, int samplerNumber) {
    BaseTrackPlayer* pSource = m_pPlayerManager->getPlayer(sourceGroup);
    if (!pSource) {
        qWarning() << "sampleLoopToSampler: no player for" << sourceGroup;
        return 0;
    }
    const TrackPointer pTrack = pSource->getLoadedTrack();
    if (!pTrack) {
        qWarning() << "sampleLoopToSampler: no track on" << sourceGroup;
        return 0;
    }

    const double loopStart = ControlObject::get(
            ConfigKey(sourceGroup, QStringLiteral("loop_start_position")));
    const double loopEnd = ControlObject::get(
            ConfigKey(sourceGroup, QStringLiteral("loop_end_position")));
    if (loopStart < 0 || loopEnd <= loopStart) {
        qWarning() << "sampleLoopToSampler: no loop set on" << sourceGroup;
        return 0;
    }

    int samplerIndex = samplerNumber - 1;
    if (samplerIndex < 0) {
        samplerIndex = 0;
        bool foundEmpty = false;
        for (int i = 0; i < m_pPlayerManager->numberOfSamplers(); ++i) {
            Sampler* pSampler = m_pPlayerManager->getSampler(i);
            if (pSampler && !pSampler->getLoadedTrack()) {
                samplerIndex = i;
                foundEmpty = true;
                break;
            }
        }
        if (!foundEmpty && m_pPlayerManager->numberOfSamplers() <= 0) {
            return 0;
        }
    }
    if (samplerIndex < 0 || samplerIndex >= m_pPlayerManager->numberOfSamplers()) {
        qWarning() << "sampleLoopToSampler: invalid sampler" << samplerNumber;
        return 0;
    }

    const QString destGroup = PlayerManager::groupForSampler(samplerIndex);
    BaseTrackPlayer* pDest = m_pPlayerManager->getPlayer(destGroup);
    if (!pDest) {
        return 0;
    }

    if (pDest->getLoadedTrack() == pTrack) {
        applyLoopToSampler(destGroup, loopStart, loopEnd);
        return samplerIndex + 1;
    }

    QObject::connect(pDest,
            &BaseTrackPlayer::newTrackLoaded,
            this,
            [destGroup, loopStart, loopEnd](TrackPointer) {
                applyLoopToSampler(destGroup, loopStart, loopEnd);
            },
            static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));

    m_pPlayerManager->slotLoadTrackToPlayer(pTrack, destGroup,
#ifdef __STEM__
            mixxx::StemChannelSelection(),
#endif
            false);
    return samplerIndex + 1;
}

void QmlPlayerManagerProxy::showNoDeckPassthroughInputConfiguredWarning() {
    emit m_pPlayerManager->noDeckPassthroughInputConfigured();
}

void QmlPlayerManagerProxy::showNoVinylControlInputConfiguredWarning() {
    emit m_pPlayerManager->noVinylControlInputConfigured();
}

QStringList QmlPlayerManagerProxy::supportedAudioFileNameFilters() const {
    return {tr("Audio (%1)")
                    .arg(SoundSourceProxy::getSupportedFileNamePatterns().join(QLatin1Char(' ')))};
}

QUrl QmlPlayerManagerProxy::initialTrackDirectoryUrl() const {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    return pConfig
            ? QUrl::fromLocalFile(pConfig->getValueString(
                      mixxx::library::prefs::kLegacyDirectoryConfigKey))
            : QUrl();
}

// static
QmlPlayerManagerProxy* QmlPlayerManagerProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pPlayerManager) {
        qWarning() << "PlayerManager hasn't been registered yet";
        return nullptr;
    }
    return new QmlPlayerManagerProxy(s_pPlayerManager, pQmlEngine);
}

} // namespace qml
} // namespace mixxx
