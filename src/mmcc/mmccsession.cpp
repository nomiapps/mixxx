#include "mmcc/mmccsession.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTcpSocket>
#include <QVariant>
#include <cmath>

#include "mmcc/mmccadapter.h"
#include "moc_mmccsession.cpp"
#include "util/fpclassify.h"

namespace mmcc {

namespace {

const QString kAdapterId = QStringLiteral("mixxx");
const QString kClientId = QStringLiteral("mixxx-adapter");
const QString kAdapterName = QStringLiteral("Mixxx MMCC Adapter");

constexpr int kMaxIdChars = 128;
constexpr int kMaxPathChars = 512;

const QString kUnsupportedPath = QStringLiteral("unsupported_path");
const QString kInvalidCommand = QStringLiteral("invalid_command");

// Sanitized, stable messages: never echo identifiers, paths or values.
QString errorMessage(const QString& code) {
    if (code == kUnsupportedPath) {
        return QStringLiteral("Target path is unsupported or not writable");
    }
    if (code == QStringLiteral("stale_object")) {
        return QStringLiteral("Target object is not available");
    }
    if (code == QStringLiteral("invalid_value")) {
        return QStringLiteral("Value is invalid for target path");
    }
    if (code == QStringLiteral("conflict")) {
        return QStringLiteral("Target rejects the command in its current state");
    }
    return QStringLiteral("Command parameters or sequence invalid");
}

const QString kStaleSequenceMessage = QStringLiteral("Command sequence is stale");
const QString kReusedIdMessage =
        QStringLiteral("Command identifier was reused with different content");

/// Walks one line of JSON that Qt has already accepted and notes, for the
/// members the contract wants as integers, whether the number was written as
/// one. Qt's reader cannot tell: it turns `1.0` into the integer 1.
///
/// Looks at the members of the top-level object and of its `protocol` object
/// only, and keeps nothing but a few flags.
class IntegerForms {
  public:
    explicit IntegerForms(const QByteArray& line)
            : m_p(line.constData()),
              m_end(line.constData() + line.size()) {
        skipSpace();
        if (m_p < m_end && *m_p == '{') {
            scanObject(QString(), 0);
        }
    }

    /// True when the last member of that name was written as an integer:
    /// an optional minus and digits, no fraction and no exponent.
    bool isInteger(const QString& key) const {
        return m_integerKeys.contains(key);
    }

  private:
    static bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    void skipSpace() {
        while (m_p < m_end && isSpace(*m_p)) {
            ++m_p;
        }
    }

    QString scanString() {
        const char* pStart = m_p;
        bool escaped = false;
        ++m_p; // opening quote
        while (m_p < m_end && *m_p != '"') {
            if (*m_p == '\\') {
                escaped = true;
                ++m_p;
            }
            ++m_p;
        }
        ++m_p; // closing quote
        const QByteArray token(pStart, static_cast<qsizetype>(qMin(m_p, m_end) - pStart));
        if (!escaped) {
            return QString::fromUtf8(token.mid(1, token.size() - 2));
        }
        // A key may be spelled with escapes; let Qt decode it.
        return QJsonDocument::fromJson('[' + token + ']').array().at(0).toString();
    }

    void scanValue(const QString& key, int depth) {
        skipSpace();
        if (m_p >= m_end) {
            return;
        }
        if (*m_p == '{') {
            scanObject(key, depth + 1);
        } else if (*m_p == '[') {
            ++m_p;
            skipSpace();
            while (m_p < m_end && *m_p != ']') {
                scanValue(QString(), depth + 2);
                skipSpace();
                if (m_p < m_end && *m_p == ',') {
                    ++m_p;
                }
                skipSpace();
            }
            ++m_p;
        } else if (*m_p == '"') {
            scanString();
        } else {
            const char* pStart = m_p;
            bool integer = *m_p == '-' || (*m_p >= '0' && *m_p <= '9');
            while (m_p < m_end && *m_p != ',' && *m_p != '}' && *m_p != ']' && !isSpace(*m_p)) {
                if (*m_p == '.' || *m_p == 'e' || *m_p == 'E') {
                    integer = false;
                }
                ++m_p;
            }
            if (!key.isEmpty() && m_p > pStart) {
                if (integer) {
                    m_integerKeys.insert(key);
                } else {
                    m_integerKeys.remove(key);
                }
            }
        }
    }

    void scanObject(const QString& prefix, int depth) {
        ++m_p; // opening brace
        skipSpace();
        while (m_p < m_end && *m_p != '}') {
            if (*m_p != '"') {
                return;
            }
            QString key = scanString();
            skipSpace();
            if (m_p < m_end && *m_p == ':') {
                ++m_p;
            }
            // Only the top level and `protocol` are of interest.
            if (depth == 1 && prefix == QStringLiteral("protocol")) {
                key = prefix + QChar('.') + key;
            } else if (depth != 0) {
                key.clear();
            }
            if (!key.isEmpty()) {
                m_integerKeys.remove(key);
            }
            scanValue(key, depth);
            skipSpace();
            if (m_p < m_end && *m_p == ',') {
                ++m_p;
            }
            skipSpace();
        }
        ++m_p;
    }

    const char* m_p;
    const char* m_end;
    QSet<QString> m_integerKeys;
};

/// A JSON integer: `1`, not `1.0` and not `true`.
bool isStrictInt(const QJsonValue& value, const IntegerForms& forms, const QString& key) {
    if (!value.isDouble() || !forms.isInteger(key)) {
        return false;
    }
    // Integers Qt could not keep exactly (beyond 64 bits) are not integers.
    const int typeId = value.toVariant().typeId();
    return typeId == QMetaType::LongLong || typeId == QMetaType::ULongLong ||
            typeId == QMetaType::Int || typeId == QMetaType::UInt;
}

int lengthInCharacters(const QString& text) {
    return text.size() <= kMaxIdChars ? static_cast<int>(text.size())
                                      : static_cast<int>(text.toUcs4().size());
}

bool isValidId(const QJsonValue& value) {
    if (!value.isString()) {
        return false;
    }
    const QString id = value.toString();
    return !id.isEmpty() && lengthInCharacters(id) <= kMaxIdChars;
}

bool isValidPath(const QJsonValue& value) {
    if (!value.isString()) {
        return false;
    }
    const QString path = value.toString();
    if (path.isEmpty() || path.size() > kMaxPathChars) {
        return false;
    }
    for (const QChar c : path) {
        if (c.unicode() < 32 || c.unicode() == 127) {
            return false;
        }
    }
    return true;
}

bool jsonIsFinite(const QJsonValue& value) {
    if (value.isDouble()) {
        return util_isfinite(value.toDouble());
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array) {
            if (!jsonIsFinite(item)) {
                return false;
            }
        }
    } else if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!jsonIsFinite(it.value())) {
                return false;
            }
        }
    }
    return true;
}

/// A fixed-size identity of a command's content for the replay comparison.
/// The canonical JSON keeps booleans apart from numbers and an absent value
/// apart from null. `1` and `1.0` are the same content here: the contract
/// leaves that to the adapter.
QByteArray commandFingerprint(const QJsonObject& message) {
    const bool hasValue = message.contains(QStringLiteral("value"));
    QJsonArray content;
    content.append(message.value(QStringLiteral("path")));
    content.append(message.value(QStringLiteral("operation")));
    content.append(hasValue);
    content.append(hasValue ? message.value(QStringLiteral("value")) : QJsonValue());
    return QCryptographicHash::hash(
            QJsonDocument(content).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256);
}

} // namespace

MmccSession::MmccSession(MmccAdapter* pAdapter, QTcpSocket* pSocket)
        : QObject(pAdapter),
          m_pAdapter(pAdapter),
          m_pSocket(pSocket),
          m_closed(false),
          m_helloReceived(false),
          m_nextSequence(0),
          m_commandSequenceWatermark(-1) {
    m_pSocket->setParent(this);
    // One line and its newline fit; more than that without a newline is a
    // protocol error, so the inbound buffer never grows past the line limit.
    m_pSocket->setReadBufferSize(m_pAdapter->config().maxLineBytes + 1);
    connect(m_pSocket, &QTcpSocket::readyRead, this, &MmccSession::slotReadyRead);
    connect(m_pSocket, &QTcpSocket::bytesWritten, this, &MmccSession::slotBytesWritten);
    connect(m_pSocket, &QTcpSocket::disconnected, this, &MmccSession::slotDisconnected);
    m_pendingTimer.setSingleShot(true);
    connect(&m_pendingTimer, &QTimer::timeout, this, &MmccSession::slotPendingTimeout);
}

MmccSession::~MmccSession() {
    // Never touches the adapter: it may be half destroyed.
    m_closed = true;
    m_pSocket->disconnect(this);
    m_pSocket->abort();
}

void MmccSession::close() {
    if (m_closed) {
        return;
    }
    m_closed = true;
    m_pending.reset();
    m_pendingTimer.stop();
    m_outbound.clear();
    m_subscriptions.clear();
    m_commandCache.clear();
    m_commandOrder.clear();
    m_pSocket->disconnect(this);
    m_pSocket->abort();
    m_pAdapter->sessionClosed(this);
}

void MmccSession::slotDisconnected() {
    close();
}

// --- Inbound ---

void MmccSession::slotReadyRead() {
    const qint64 maxLineBytes = m_pAdapter->config().maxLineBytes;
    // While a command waits for the engine, the next lines stay in the
    // socket's buffer, which is bounded by the line limit.
    while (!m_closed && !m_pending) {
        if (!m_pSocket->canReadLine()) {
            if (m_pSocket->bytesAvailable() > maxLineBytes) {
                close();
            }
            return;
        }
        QByteArray line = m_pSocket->readLine(maxLineBytes + 2);
        if (!line.endsWith('\n')) {
            // Longer than the limit.
            close();
            return;
        }
        line.chop(1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.size() > maxLineBytes || !processLine(line)) {
            close();
            return;
        }
    }
}

bool MmccSession::processLine(const QByteArray& line) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject message = document.object();
    if (!jsonIsFinite(message)) {
        return false;
    }

    const QJsonValue protocol = message.value(QStringLiteral("protocol"));
    if (!protocol.isObject()) {
        return false;
    }
    const IntegerForms forms(line);
    const QJsonValue major = protocol.toObject().value(QStringLiteral("major"));
    if (!isStrictInt(major, forms, QStringLiteral("protocol.major")) || major.toInteger() != 1) {
        return false;
    }

    const QJsonValue kindValue = message.value(QStringLiteral("kind"));
    if (!kindValue.isString()) {
        return false;
    }
    const QString kind = kindValue.toString();

    // Over-long identifiers close the connection: they cannot be echoed.
    for (const QString& key : {QStringLiteral("client_id"),
                 QStringLiteral("subscription_id"),
                 QStringLiteral("command_id")}) {
        const QJsonValue id = message.value(key);
        if (id.isString() && lengthInCharacters(id.toString()) > kMaxIdChars) {
            return false;
        }
    }

    if (kind == QStringLiteral("hello")) {
        m_helloReceived = true;
        m_pAdapter->flushDiscrete();
        sendHandshake();
        return true;
    }
    // Anything else before hello closes the connection without a reply.
    if (!m_helloReceived) {
        return false;
    }
    // Whatever changed in Mixxx up to now is reported before this message is
    // answered, so that a reply never describes an older state than the
    // events that follow it.
    m_pAdapter->flushDiscrete();
    if (m_closed) {
        return true;
    }
    if (kind == QStringLiteral("subscribe")) {
        return processSubscribe(message);
    }
    if (kind == QStringLiteral("unsubscribe")) {
        const QJsonValue id = message.value(QStringLiteral("subscription_id"));
        if (id.isString()) {
            m_subscriptions.remove(id.toString());
        }
        return true;
    }
    if (kind == QStringLiteral("heartbeat")) {
        QJsonValue monotonicMs(0);
        if (message.contains(QStringLiteral("monotonic_ms"))) {
            monotonicMs = message.value(QStringLiteral("monotonic_ms"));
            if (!isStrictInt(monotonicMs, forms, QStringLiteral("monotonic_ms")) ||
                    monotonicMs.toInteger() < 0) {
                return false;
            }
        }
        QJsonObject reply = frame(QStringLiteral("heartbeat"));
        reply.insert(QStringLiteral("monotonic_ms"), monotonicMs);
        enqueue(reply);
        return true;
    }
    if (kind == QStringLiteral("command")) {
        return processCommand(message,
                !message.contains(QStringLiteral("generation")) ||
                        isStrictInt(message.value(QStringLiteral("generation")),
                                forms,
                                QStringLiteral("generation")),
                isStrictInt(message.value(QStringLiteral("sequence")),
                        forms,
                        QStringLiteral("sequence")));
    }
    return false;
}

bool MmccSession::processSubscribe(const QJsonObject& message) {
    const MmccAdapterConfig& config = m_pAdapter->config();
    const QJsonValue idValue = message.value(QStringLiteral("subscription_id"));
    const QJsonValue pathsValue = message.value(QStringLiteral("paths"));
    // Everything is validated before any part of the request is kept.
    if (!isValidId(idValue) || !pathsValue.isArray()) {
        return false;
    }
    const QJsonArray paths = pathsValue.toArray();
    if (paths.isEmpty()) {
        return false;
    }
    QSet<int> entries;
    entries.reserve(config.maxPathsPerSubscription + 1);
    for (const QJsonValue& path : paths) {
        if (!isValidPath(path)) {
            return false;
        }
        const int entryIndex = m_pAdapter->readableIndex(path.toString());
        if (entryIndex < 0) {
            return false;
        }
        entries.insert(entryIndex);
        // Unique paths are what counts; stop as soon as the limit is passed
        // so a long list cannot make this set large.
        if (entries.size() > config.maxPathsPerSubscription) {
            return false;
        }
    }
    const QString id = idValue.toString();
    // A live subscription is never evicted to admit another.
    if (!m_subscriptions.contains(id) &&
            m_subscriptions.size() >= config.maxSubscriptionsPerSession) {
        return false;
    }
    qsizetype others = 0;
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        if (it.key() != id) {
            others += it.value().size();
        }
    }
    if (others + entries.size() > config.maxTotalSubscribedPaths) {
        return false;
    }
    m_subscriptions.insert(id, entries);
    sendSnapshot();
    return true;
}

bool MmccSession::processCommand(
        const QJsonObject& message, bool generationIsInteger, bool sequenceIsInteger) {
    const QJsonValue idValue = message.value(QStringLiteral("command_id"));
    if (!isValidId(idValue)) {
        return false;
    }
    const QString commandId = idValue.toString();

    // Order of judgement: generation, sequence and replay, operation, path,
    // `set` without a value, then the value, the object and its state.
    if (message.contains(QStringLiteral("generation"))) {
        const QJsonValue generation = message.value(QStringLiteral("generation"));
        if (!generationIsInteger || generation.toInteger() != m_pAdapter->generation()) {
            sendResult(resultError(commandId, kInvalidCommand));
            return true;
        }
    }

    const QJsonValue sequenceValue = message.value(QStringLiteral("sequence"));
    if (!sequenceIsInteger || sequenceValue.toInteger() < 0) {
        sendResult(resultError(commandId, kInvalidCommand));
        return true;
    }
    const qint64 sequence = sequenceValue.toInteger();

    // 1. A retained command_id is judged by its content, whatever its
    //    sequence: the same content returns the retained result and applies
    //    nothing.
    const QByteArray fingerprint = commandFingerprint(message);
    const auto cached = m_commandCache.constFind(commandId);
    if (cached != m_commandCache.constEnd()) {
        m_commandSequenceWatermark = qMax(m_commandSequenceWatermark, sequence);
        if (cached->fingerprint == fingerprint) {
            sendResult(cached->result);
        } else {
            sendResult(resultError(commandId, kInvalidCommand, kReusedIdMessage));
        }
        return true;
    }

    // 2. Forgetting a result never makes an old sequence acceptable again.
    if (sequence <= m_commandSequenceWatermark) {
        sendResult(resultError(commandId, kInvalidCommand, kStaleSequenceMessage));
        return true;
    }

    // 3. A new command.
    m_commandSequenceWatermark = sequence;

    const QJsonValue operation = message.value(QStringLiteral("operation"));
    const QJsonValue path = message.value(QStringLiteral("path"));
    const bool isSet = operation.toString() == QStringLiteral("set");
    const bool isInvoke = operation.toString() == QStringLiteral("invoke");
    const int entryIndex = isValidPath(path) ? m_pAdapter->writableIndex(path.toString()) : -1;

    QJsonObject result;
    if (!isSet && !isInvoke) {
        result = resultError(commandId, kInvalidCommand);
    } else if (entryIndex < 0 || isInvoke) {
        // No path takes `invoke` in this pass.
        result = resultError(commandId, kUnsupportedPath);
    } else if (!message.contains(QStringLiteral("value"))) {
        result = resultError(commandId, kInvalidCommand);
    } else {
        const MmccAdapter::CommandOutcome outcome =
                m_pAdapter->execute(entryIndex, message.value(QStringLiteral("value")));
        if (outcome.pending) {
            m_pending = PendingCommand{commandId, fingerprint, entryIndex, outcome.target};
            m_pendingTimer.start(m_pAdapter->config().engineApplyTimeoutMs);
            return true;
        }
        result = outcome.errorCode.isEmpty()
                ? resultOk(commandId, outcome.effectiveValue)
                : resultError(commandId, outcome.errorCode);
    }
    // Failures are retained too.
    cacheResult(commandId, fingerprint, result);
    // The result goes out before any event that reports its effect.
    sendResult(result);
    m_pAdapter->flushDiscrete();
    return true;
}

void MmccSession::resolvePending(bool timedOut) {
    if (m_closed || !m_pending) {
        return;
    }
    const bool taken = m_pAdapter->hasTaken(m_pending->entryIndex, m_pending->target);
    if (!taken && !timedOut) {
        return;
    }
    const PendingCommand pending = *m_pending;
    m_pending.reset();
    m_pendingTimer.stop();
    // Mixxx did not take the value in time: conflict, and nothing is reported.
    const QJsonObject result = taken
            ? resultOk(pending.commandId,
                      m_pAdapter->effectiveValue(pending.entryIndex, pending.target))
            : resultError(pending.commandId, QStringLiteral("conflict"));
    cacheResult(pending.commandId, pending.fingerprint, result);
    sendResult(result);
    // Carry on with the lines that arrived in the meantime.
    QMetaObject::invokeMethod(this, &MmccSession::slotReadyRead, Qt::QueuedConnection);
}

void MmccSession::slotPendingTimeout() {
    resolvePending(true);
    if (!m_closed) {
        m_pAdapter->flushDiscrete();
    }
}

void MmccSession::cacheResult(const QString& commandId,
        const QByteArray& fingerprint,
        const QJsonObject& result) {
    // Reclaim before admitting at the cap, so the cache never exceeds it.
    const int cap = m_pAdapter->config().maxCommandsPerSession;
    while (m_commandOrder.size() >= cap) {
        m_commandCache.remove(m_commandOrder.dequeue());
    }
    m_commandCache.insert(commandId, CachedResult{fingerprint, result});
    m_commandOrder.enqueue(commandId);
}

// --- Outbound ---

QJsonObject MmccSession::frame(const QString& kind) {
    QJsonObject protocol;
    protocol.insert(QStringLiteral("major"), 1);
    protocol.insert(QStringLiteral("minor"), 0);
    QJsonObject object;
    object.insert(QStringLiteral("protocol"), protocol);
    object.insert(QStringLiteral("kind"), kind);
    object.insert(QStringLiteral("client_id"), kClientId);
    // Replies, events and heartbeats share one sequence, numbered in the
    // order the frames are queued for the socket.
    object.insert(QStringLiteral("sequence"), m_nextSequence++);
    object.insert(QStringLiteral("generation"), m_pAdapter->generation());
    return object;
}

QJsonObject MmccSession::resultOk(
        const QString& commandId, const QJsonValue& effectiveValue) const {
    QJsonObject result;
    result.insert(QStringLiteral("command_id"), commandId);
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("effective_value"), effectiveValue);
    return result;
}

QJsonObject MmccSession::resultError(
        const QString& commandId, const QString& code, const QString& message) const {
    QJsonObject error;
    error.insert(QStringLiteral("code"), code);
    error.insert(QStringLiteral("message"), message.isEmpty() ? errorMessage(code) : message);
    QJsonObject result;
    result.insert(QStringLiteral("command_id"), commandId);
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), error);
    return result;
}

void MmccSession::sendResult(const QJsonObject& result) {
    QJsonObject object = frame(QStringLiteral("result"));
    for (auto it = result.constBegin(); it != result.constEnd(); ++it) {
        object.insert(it.key(), it.value());
    }
    enqueue(object);
}

void MmccSession::sendHandshake() {
    QJsonObject hello = frame(QStringLiteral("hello"));
    hello.insert(QStringLiteral("role"), QStringLiteral("application_adapter"));
    hello.insert(QStringLiteral("name"), kAdapterName);
    hello.insert(QStringLiteral("instance_id"),
            QStringLiteral("mixxx-%1").arg(m_pAdapter->generation()));
    enqueue(hello);

    QJsonObject capabilities = frame(QStringLiteral("capabilities"));
    capabilities.insert(QStringLiteral("features"),
            QJsonArray::fromStringList({QStringLiteral("deck_mixer"),
                    QStringLiteral("deck_transport"),
                    QStringLiteral("deck_meters"),
                    QStringLiteral("stems"),
                    QStringLiteral("quick_effect"),
                    QStringLiteral("crossfader"),
                    QStringLiteral("effect_units"),
                    QStringLiteral("samplers")}));
    capabilities.insert(QStringLiteral("readable_paths"),
            QJsonArray::fromStringList(m_pAdapter->readablePaths()));
    capabilities.insert(QStringLiteral("writable_paths"),
            QJsonArray::fromStringList(m_pAdapter->writablePaths()));
    capabilities.insert(QStringLiteral("meter_rate_hz"), m_pAdapter->meterRateHz());
    enqueue(capabilities);

    sendSnapshot();
}

void MmccSession::sendSnapshot() {
    QJsonObject snapshot = frame(QStringLiteral("snapshot"));
    snapshot.insert(QStringLiteral("adapter"), kAdapterId);
    snapshot.insert(QStringLiteral("revision"), m_pAdapter->revision());
    snapshot.insert(QStringLiteral("values"), m_pAdapter->snapshotValues());
    enqueue(snapshot);
}

void MmccSession::sendEvents(qint64 revision,
        const QList<int>& changedEntries,
        const QJsonObject& values) {
    const QList<PathEntry>& entries = m_pAdapter->entries();
    // The subscriptions are copied: a full queue closes the session, which
    // clears m_subscriptions while it is being walked.
    const QHash<QString, QSet<int>> subscriptions = m_subscriptions;
    for (auto it = subscriptions.constBegin(); it != subscriptions.constEnd(); ++it) {
        QJsonObject changes;
        for (int entryIndex : changedEntries) {
            if (it.value().contains(entryIndex)) {
                const QString& path = entries.at(entryIndex).path;
                changes.insert(path, values.value(path));
            }
        }
        if (changes.isEmpty()) {
            continue;
        }
        if (m_closed) {
            return;
        }
        QJsonObject event = frame(QStringLiteral("event"));
        event.insert(QStringLiteral("adapter"), kAdapterId);
        event.insert(QStringLiteral("revision"), revision);
        event.insert(QStringLiteral("subscription_id"), it.key());
        event.insert(QStringLiteral("changes"), changes);
        enqueue(event);
    }
}

void MmccSession::enqueue(const QJsonObject& frame) {
    if (m_closed) {
        return;
    }
    // A peer that does not read is closed rather than buffered without limit.
    if (m_outbound.size() >= m_pAdapter->config().maxOutboundQueueSize) {
        close();
        return;
    }
    QByteArray line = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    line.append('\n');
    m_outbound.enqueue(line);
    m_pAdapter->noteOutboundQueueSize(static_cast<int>(m_outbound.size()));
    pump();
}

void MmccSession::pump() {
    const qint64 highWater = m_pAdapter->config().socketHighWaterBytes;
    while (!m_closed && !m_outbound.isEmpty() && m_pSocket->bytesToWrite() < highWater) {
        if (m_pSocket->write(m_outbound.dequeue()) < 0) {
            close();
            return;
        }
    }
}

void MmccSession::slotBytesWritten() {
    pump();
}

} // namespace mmcc
