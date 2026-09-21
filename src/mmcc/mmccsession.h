#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QTimer>
#include <optional>

class QTcpSocket;

namespace mmcc {

class MmccAdapter;

/// One connection of the MMCC adapter: NDJSON framing, the `hello` gate,
/// subscriptions, the command replay cache and the bounded outbound queue.
///
/// Every retained structure here has its own cap, is validated before
/// anything is kept and reclaims at the cap (MMCC AGENTS.md, "Bounded
/// retained storage"). Lives on the adapter's thread.
class MmccSession : public QObject {
    Q_OBJECT
  public:
    /// Takes ownership of `pSocket`.
    MmccSession(MmccAdapter* pAdapter, QTcpSocket* pSocket);
    ~MmccSession() override;

    bool isOpen() const {
        return !m_closed;
    }
    bool helloReceived() const {
        return m_helloReceived;
    }

    /// Sends one event per subscription that holds any of the changed
    /// entries. `values` maps a path to its new value.
    void sendEvents(qint64 revision,
            const QList<int>& changedEntries,
            const QJsonObject& values);

    /// Drops the connection without a reply. Safe to call more than once.
    void close();

    /// Answers the command that waits for the engine thread, if there is one
    /// and the control has taken its value, or if `timedOut`. Called by the
    /// adapter before it reports changes.
    void resolvePending(bool timedOut);
    bool hasPendingCommand() const {
        return m_pending.has_value();
    }

    int commandCacheSize() const {
        return static_cast<int>(m_commandCache.size());
    }
    int subscriptionCount() const {
        return static_cast<int>(m_subscriptions.size());
    }
    int outboundQueueSize() const {
        return static_cast<int>(m_outbound.size());
    }

  private slots:
    void slotReadyRead();
    void slotBytesWritten();
    void slotDisconnected();
    void slotPendingTimeout();

  private:
    /// The one command whose control the engine thread has yet to apply. The
    /// session reads nothing further until it is answered, so there is never
    /// more than one and results keep the order of their commands.
    struct PendingCommand {
        QString commandId;
        QByteArray fingerprint;
        int entryIndex;
        double target;
    };
    struct CachedResult {
        /// SHA-256 of the command's content: never the command itself, a
        /// path or a value can be as long as a line.
        QByteArray fingerprint;
        /// The result without `sequence`; bounded by the identifier limit.
        QJsonObject result;
    };

    /// False when the line is a protocol error and the session was closed.
    bool processLine(const QByteArray& line);
    bool processSubscribe(const QJsonObject& message);
    /// Qt reads `1.0` as the integer 1, so the caller says whether the two
    /// numbers were written as integers.
    bool processCommand(const QJsonObject& message,
            bool generationIsInteger,
            bool sequenceIsInteger);
    void sendHandshake();
    void sendSnapshot();

    QJsonObject frame(const QString& kind);
    QJsonObject resultOk(const QString& commandId, const QJsonValue& effectiveValue) const;
    QJsonObject resultError(const QString& commandId,
            const QString& code,
            const QString& message = QString()) const;
    void sendResult(const QJsonObject& result);
    void cacheResult(const QString& commandId,
            const QByteArray& fingerprint,
            const QJsonObject& result);
    void enqueue(const QJsonObject& frame);
    void pump();

    MmccAdapter* const m_pAdapter;
    QTcpSocket* const m_pSocket;
    bool m_closed;
    bool m_helloReceived;
    qint64 m_nextSequence;
    qint64 m_commandSequenceWatermark;

    /// subscription_id -> entry indices. At most maxSubscriptionsPerSession
    /// ids of at most 128 characters and maxPathsPerSubscription entries.
    QHash<QString, QSet<int>> m_subscriptions;
    /// command_id -> retained result, oldest first in m_commandOrder. Both
    /// hold exactly the same ids, at most maxCommandsPerSession.
    QHash<QString, CachedResult> m_commandCache;
    QQueue<QString> m_commandOrder;
    /// Frames not yet handed to the socket, at most maxOutboundQueueSize.
    QQueue<QByteArray> m_outbound;
    std::optional<PendingCommand> m_pending;
    QTimer m_pendingTimer;
};

} // namespace mmcc
