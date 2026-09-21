#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "control/pollingcontrolproxy.h"
#include "mmcc/mmccpathtable.h"
#include "track/track_decl.h"

class ControlProxy;
class PlayerInfo;
class QTcpServer;

namespace mmcc {

class MmccSession;

constexpr quint16 kDefaultPort = 47021;
constexpr int kMinMeterRateHz = 1;
constexpr int kMaxMeterRateHz = 60;
constexpr int kDefaultMeterRateHz = 24;

/// Every bound of the adapter. All of them are validated before anything is
/// allocated; see `MmccAdapter::Config::isValid`.
struct MmccAdapterConfig {
    /// 0 asks the operating system for a free port (tests).
    quint16 port = kDefaultPort;
    int meterRateHz = kDefaultMeterRateHz;
    /// Sessions. A closed session is reclaimed at once; a live session is
    /// never evicted, so one more connection than this is refused.
    int maxSessions = 8;
    /// Retained command results per session; the oldest is evicted first.
    int maxCommandsPerSession = 100;
    int maxSubscriptionsPerSession = 32;
    int maxPathsPerSubscription = 16;
    int maxTotalSubscribedPaths = 256;
    /// Frames waiting for one connection's socket. A session whose queue is
    /// full is closed rather than allowed to grow.
    int maxOutboundQueueSize = 256;
    /// Frames are handed to the socket only while it holds fewer unsent
    /// bytes than this, so a peer that stops reading fills the queue above
    /// instead of Qt's unbounded write buffer.
    qint64 socketHighWaterBytes = 256 * 1024;
    /// One inbound line, without its newline.
    qint64 maxLineBytes = 1024 * 1024;
    /// How long the read-back of a control that the engine thread applies
    /// may wait before the command is answered `conflict`.
    int engineApplyTimeoutMs = 500;
    /// A new value for each Mixxx start. 0 derives it from the wall clock.
    qint64 generation = 0;
    /// Monotonic nanoseconds for the meter rate gate. Empty uses the
    /// steady clock; tests inject their own.
    std::function<qint64()> clockNs;
    /// False leaves polling to the caller (tests call `pollContinuous`).
    bool startMeterTimer = true;

    bool isValid() const;
};

/// MMCC protocol v1 application adapter: a loopback NDJSON server the MMCC
/// hub dials into, exposing ControlObjects as the semantic paths of
/// `docs/mixxx-adapter-paths.md` (MMCC repository).
///
/// Threading: this object, its sessions and every control access live on the
/// thread that created it (the main thread). Discrete controls are observed
/// through `ControlProxy`, whose `valueChanged` arrives on this thread;
/// meters and positions are never listened to, they are read through
/// `PollingControlProxy` from one timer. Nothing here runs on, locks against
/// or allocates on the engine thread.
///
/// Lifetime: destroy it before the engine, the players and `PlayerInfo`.
/// The destructor stops listening and closes every socket first.
class MmccAdapter : public QObject {
    Q_OBJECT
  public:
    /// `pPlayerInfo` supplies the loaded tracks (title and artist are not
    /// controls). It may be null, then no deck ever reports a track.
    MmccAdapter(const MmccAdapterConfig& config,
            PlayerInfo* pPlayerInfo,
            QObject* pParent = nullptr);
    ~MmccAdapter() override;

    bool isListening() const;
    quint16 serverPort() const;

    qint64 generation() const {
        return m_config.generation;
    }
    qint64 revision() const {
        return m_revision;
    }
    int meterRateHz() const {
        return m_meterRateHz;
    }
    /// Bounded and validated. A rejected rate leaves the previous one.
    bool setMeterRateHz(int rateHz);

    int sessionCount() const {
        return static_cast<int>(m_sessions.size());
    }
    /// The largest outbound queue any session has had (tests).
    int peakOutboundQueueSize() const {
        return m_peakOutboundQueue;
    }

    const MmccAdapterConfig& config() const {
        return m_config;
    }
    const QList<PathEntry>& entries() const {
        return m_entries;
    }
    const QStringList& readablePaths() const {
        return m_readablePaths;
    }
    const QStringList& writablePaths() const {
        return m_writablePaths;
    }
    /// Index into `entries()`, or -1.
    int readableIndex(const QString& path) const {
        return m_readableIndex.value(path, -1);
    }
    int writableIndex(const QString& path) const {
        return m_writableIndex.value(path, -1);
    }

    /// Every readable path with the value last reported for it.
    QJsonObject snapshotValues() const;

    struct CommandOutcome {
        /// Empty on success, otherwise a protocol v1 error code.
        QString errorCode;
        QJsonValue effectiveValue;
        /// The value was written, but the engine thread has not applied it
        /// yet: ask `hasTaken(entryIndex, target)` again later.
        bool pending = false;
        double target = 0.0;
    };
    /// Judges and applies `set value` on a writable entry: `invalid_value`,
    /// `stale_object`, `conflict`, then write and read back. Reports nothing;
    /// the caller sends the result and then calls `flushDiscrete`.
    CommandOutcome execute(int entryIndex, const QJsonValue& value);
    /// Whether the entry's control now reads back as `target`.
    bool hasTaken(int entryIndex, double target) const;
    /// `target` as the path reports it: a boolean or a number.
    QJsonValue effectiveValue(int entryIndex, double target) const;

    /// Reports every discrete path whose value changed since it was last
    /// reported, under one revision. Also reports continuous paths that
    /// moved between `null` and a value, which is part of a load or eject.
    void flushDiscrete();

    /// Samples meters and positions and emits at most one meter frame per
    /// `1 / meterRateHz` seconds. Called by the timer; tests call it.
    void pollContinuous();

    /// Called by a session when its connection is gone.
    void sessionClosed(MmccSession* pSession);
    void noteOutboundQueueSize(int size);

  private slots:
    void slotNewConnection();
    void slotDiscreteControlChanged();
    void slotCountsChanged();
    void slotTrackChanged(const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack);

  private:
    struct EntryState {
        /// Discrete control sources; owned by this QObject.
        std::vector<ControlProxy*> proxies;
        /// Continuous control source.
        std::optional<PollingControlProxy> polled;
        /// Value last reported in a snapshot or event.
        QJsonValue reported;
    };

    void ensureProxies();
    bool ownerIndexInRange(const PathEntry& entry) const;
    bool entryResolved(int entryIndex) const;
    int numDecks() const;
    int numSamplers() const;
    bool deckAvailable(int deck) const;
    bool samplerAvailable(int sampler) const;
    bool deckLoaded(int deck) const;
    bool samplerLoaded(int sampler) const;
    int stemCount(int deck) const;
    /// Whether the entry's owner exists right now; false means `null` and
    /// `stale_object`.
    bool ownerAvailable(const PathEntry& entry) const;
    /// Whether the entry's deck or sampler holds no track.
    bool ownerEmpty(const PathEntry& entry) const;
    double readControl(int entryIndex) const;
    double readBack(int entryIndex) const;
    QJsonValue computeValue(int entryIndex) const;
    void scheduleDiscreteFlush();
    void publish(const QList<int>& changedEntries);

    MmccAdapterConfig m_config;
    const QList<PathEntry> m_entries;
    std::vector<EntryState> m_states;
    QStringList m_readablePaths;
    QStringList m_writablePaths;
    QHash<QString, int> m_readableIndex;
    QHash<QString, int> m_writableIndex;
    /// Entry index of `[ChannelN],track_loaded`, `stem_count` and
    /// `[SamplerS],track_loaded`.
    int m_deckLoadedEntry[kNumDecks];
    int m_deckStemCountEntry[kNumDecks];
    int m_samplerLoadedEntry[kNumSamplers];
    /// The tracks on the decks, as PlayerInfo announced them.
    TrackPointer m_deckTracks[kNumDecks];

    ControlProxy* m_pNumDecks;
    ControlProxy* m_pNumSamplers;

    qint64 m_revision;
    int m_meterRateHz;
    qint64 m_meterIntervalNs;
    std::optional<qint64> m_lastMeterEmitNs;
    bool m_flushScheduled;
    int m_peakOutboundQueue;

    QTimer m_meterTimer;
    QTcpServer* m_pServer;
    /// Live sessions only, at most `maxSessions`.
    QList<MmccSession*> m_sessions;
};

} // namespace mmcc
