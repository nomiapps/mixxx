#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "mixer/playermanager.h"
#include "qml/qmlplayerproxy.h"

namespace mixxx {
namespace qml {

class QmlPlayerManagerProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList supportedAudioFileNameFilters READ
                    supportedAudioFileNameFilters CONSTANT)
    Q_PROPERTY(QUrl initialTrackDirectoryUrl READ initialTrackDirectoryUrl CONSTANT)
    QML_NAMED_ELEMENT(PlayerManager)
    QML_SINGLETON
  public:
    explicit QmlPlayerManagerProxy(
            std::shared_ptr<PlayerManager> pPlayerManager,
            QObject* parent = nullptr);

    Q_INVOKABLE mixxx::qml::QmlPlayerProxy* getPlayer(const QString& deck);
    Q_INVOKABLE void loadLocationIntoNextAvailableDeck(const QString& location, bool play = false);
    Q_INVOKABLE void loadLocationUrlIntoNextAvailableDeck(
            const QUrl& locationUrl, bool play = false);
    Q_INVOKABLE void loadLocationUrlToDeck(const QUrl& locationUrl, int deck);
    Q_INVOKABLE void loadLocationToPlayer(
            const QString& location, const QString& group, bool play = false);
    Q_INVOKABLE void loadTrackToPlayer(TrackPointer track,
            const QString& group,
#ifdef __STEM__
            mixxx::StemChannelSelection stemSelection,
#endif
            bool play);
    Q_INVOKABLE void showNoDeckPassthroughInputConfiguredWarning();
    Q_INVOKABLE void showNoVinylControlInputConfiguredWarning();
    /// Copy the source deck's current loop onto a sampler: same track, loop
    /// in/out, loop enabled, cue at loop start. samplerNumber is 1-based;
    /// 0 picks the first empty sampler (or sampler 1 if all are loaded).
    /// Returns the 1-based sampler index, or 0 on failure.
    Q_INVOKABLE int sampleLoopToSampler(const QString& sourceGroup, int samplerNumber = 0);

    QStringList supportedAudioFileNameFilters() const;
    QUrl initialTrackDirectoryUrl() const;

    static QmlPlayerManagerProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerPlayerManager(std::shared_ptr<PlayerManager> pPlayerManager) {
        s_pPlayerManager = std::move(pPlayerManager);
    }

  private:
    static inline std::shared_ptr<PlayerManager> s_pPlayerManager;

    const std::shared_ptr<PlayerManager> m_pPlayerManager;
};

} // namespace qml
} // namespace mixxx
