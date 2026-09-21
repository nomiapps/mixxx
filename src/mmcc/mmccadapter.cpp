#include "mmcc/mmccadapter.h"

#include <QDateTime>
#include <QTcpServer>
#include <QTcpSocket>
#include <chrono>
#include <cmath>

#include "control/controlproxy.h"
#include "mixer/playerinfo.h"
#include "mmcc/mmccsession.h"
#include "moc_mmccadapter.cpp"
#include "track/track.h"
#include "util/assert.h"
#include "util/fpclassify.h"
#include "util/logger.h"

namespace mmcc {

namespace {

const mixxx::Logger kLogger("MmccAdapter");

const QString kAppGroup = QStringLiteral("[App]");
const QString kStatusConnected = QStringLiteral("connected");

constexpr ControlFlags kOptionalControl = ControlFlag::NoWarnIfMissing;

qint64 steadyClockNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
}

/// Cuts a tag at kMaxTagChars UTF-16 units without splitting a surrogate
/// pair. Longer strings are cut, not rejected.
QString cutTag(const QString& tag) {
    if (tag.size() <= kMaxTagChars) {
        return tag;
    }
    qsizetype length = kMaxTagChars;
    if (tag.at(length - 1).isHighSurrogate()) {
        --length;
    }
    return tag.left(length);
}

int deckIndexForGroup(const QString& group) {
    for (int deck = 0; deck < kNumDecks; ++deck) {
        if (group == QStringLiteral("[Channel%1]").arg(deck + 1)) {
            return deck;
        }
    }
    return -1;
}

} // namespace

bool MmccAdapterConfig::isValid() const {
    return meterRateHz >= kMinMeterRateHz && meterRateHz <= kMaxMeterRateHz &&
            maxSessions >= 1 && maxCommandsPerSession >= 1 &&
            maxSubscriptionsPerSession >= 1 && maxPathsPerSubscription >= 1 &&
            maxTotalSubscribedPaths >= 1 && maxOutboundQueueSize >= 1 &&
            socketHighWaterBytes >= 1 && maxLineBytes >= 1 && engineApplyTimeoutMs >= 1 &&
            generation >= 0;
}

MmccAdapter::MmccAdapter(const MmccAdapterConfig& config,
        PlayerInfo* pPlayerInfo,
        QObject* pParent)
        : QObject(pParent),
          m_config(config),
          m_entries(buildPathTable()),
          m_states(static_cast<std::size_t>(m_entries.size())),
          m_pNumDecks(nullptr),
          m_pNumSamplers(nullptr),
          m_revision(1),
          m_meterRateHz(kDefaultMeterRateHz),
          m_meterIntervalNs(1000000000 / kDefaultMeterRateHz),
          m_flushScheduled(false),
          m_peakOutboundQueue(0),
          m_pServer(nullptr) {
    // Every bound is judged before anything is opened or kept.
    const bool configValid = m_config.isValid();
    if (!configValid) {
        kLogger.warning() << "Rejected configuration, the adapter stays off";
        m_config = MmccAdapterConfig();
        m_config.clockNs = config.clockNs;
        m_config.startMeterTimer = false;
    }
    if (m_config.generation == 0) {
        m_config.generation = QDateTime::currentSecsSinceEpoch();
    }
    if (!m_config.clockNs) {
        m_config.clockNs = &steadyClockNs;
    }
    setMeterRateHz(m_config.meterRateHz);

    for (int deck = 0; deck < kNumDecks; ++deck) {
        m_deckLoadedEntry[deck] = -1;
        m_deckStemCountEntry[deck] = -1;
    }
    for (int sampler = 0; sampler < kNumSamplers; ++sampler) {
        m_samplerLoadedEntry[sampler] = -1;
    }
    m_readablePaths.reserve(kExpectedReadablePaths);
    m_writablePaths.reserve(kExpectedWritablePaths);
    for (int i = 0; i < m_entries.size(); ++i) {
        const PathEntry& entry = m_entries.at(i);
        if (entry.pRow->readable) {
            m_readablePaths.append(entry.path);
            m_readableIndex.insert(entry.path, i);
        }
        if (entry.pRow->writable) {
            m_writablePaths.append(entry.path);
            m_writableIndex.insert(entry.path, i);
        }
        const QString key = QString::fromLatin1(entry.pRow->key);
        if (entry.pRow->owner == Owner::Deck && key == QStringLiteral("track_loaded")) {
            m_deckLoadedEntry[entry.index] = i;
        } else if (entry.pRow->owner == Owner::Deck && key == QStringLiteral("stem_count")) {
            m_deckStemCountEntry[entry.index] = i;
        } else if (entry.pRow->owner == Owner::Sampler &&
                key == QStringLiteral("track_loaded")) {
            m_samplerLoadedEntry[entry.index] = i;
        }
    }
    DEBUG_ASSERT(m_readablePaths.size() == kExpectedReadablePaths);
    DEBUG_ASSERT(m_writablePaths.size() == kExpectedWritablePaths);

    m_pNumDecks = new ControlProxy(
            kAppGroup, QStringLiteral("num_decks"), this, kOptionalControl);
    m_pNumDecks->connectValueChanged(this, &MmccAdapter::slotCountsChanged);
    m_pNumSamplers = new ControlProxy(
            kAppGroup, QStringLiteral("num_samplers"), this, kOptionalControl);
    m_pNumSamplers->connectValueChanged(this, &MmccAdapter::slotCountsChanged);

    if (pPlayerInfo) {
        for (int deck = 0; deck < kNumDecks; ++deck) {
            slotTrackChanged(QStringLiteral("[Channel%1]").arg(deck + 1),
                    pPlayerInfo->getTrackInfo(QStringLiteral("[Channel%1]").arg(deck + 1)),
                    TrackPointer());
        }
        connect(pPlayerInfo,
                &PlayerInfo::trackChanged,
                this,
                &MmccAdapter::slotTrackChanged);
    }

    ensureProxies();
    for (int i = 0; i < m_entries.size(); ++i) {
        m_states[static_cast<std::size_t>(i)].reported = computeValue(i);
    }
    m_flushScheduled = false;

    if (!configValid) {
        return;
    }

    m_pServer = new QTcpServer(this);
    m_pServer->setMaxPendingConnections(m_config.maxSessions);
    connect(m_pServer, &QTcpServer::newConnection, this, &MmccAdapter::slotNewConnection);
    // Loopback only: the hub runs on this machine and dials in.
    if (!m_pServer->listen(QHostAddress::LocalHost, m_config.port)) {
        kLogger.warning() << "Cannot listen on 127.0.0.1 port" << m_config.port
                          << ":" << m_pServer->errorString();
    } else {
        kLogger.info() << "Listening on 127.0.0.1 port" << m_pServer->serverPort();
    }

    connect(&m_meterTimer, &QTimer::timeout, this, &MmccAdapter::pollContinuous);
    m_meterTimer.setTimerType(Qt::PreciseTimer);
    if (m_config.startMeterTimer) {
        m_meterTimer.start();
    }
}

MmccAdapter::~MmccAdapter() {
    // Stop accepting and drop every socket before anything this object
    // references goes away. Sessions never touch the adapter while dying.
    m_meterTimer.stop();
    if (m_pServer) {
        m_pServer->close();
    }
    const QList<MmccSession*> sessions =
            findChildren<MmccSession*>(QString(), Qt::FindDirectChildrenOnly);
    m_sessions.clear();
    for (MmccSession* pSession : sessions) {
        pSession->close();
        delete pSession;
    }
}

bool MmccAdapter::isListening() const {
    return m_pServer && m_pServer->isListening();
}

quint16 MmccAdapter::serverPort() const {
    return m_pServer ? m_pServer->serverPort() : 0;
}

bool MmccAdapter::setMeterRateHz(int rateHz) {
    if (rateHz < kMinMeterRateHz || rateHz > kMaxMeterRateHz) {
        return false;
    }
    m_meterRateHz = rateHz;
    m_meterIntervalNs = 1000000000 / rateHz;
    // Round the tick up so that the timer is never faster than the gate.
    m_meterTimer.setInterval((1000 + rateHz - 1) / rateHz);
    return true;
}

// --- Controls ---

bool MmccAdapter::ownerIndexInRange(const PathEntry& entry) const {
    switch (entry.pRow->owner) {
    case Owner::Deck:
    case Owner::Stem:
        return entry.index < numDecks();
    case Owner::Sampler:
        return entry.index < numSamplers();
    case Owner::Adapter:
    case Owner::Global:
    case Owner::EffectUnit:
        break;
    }
    return true;
}

void MmccAdapter::ensureProxies() {
    for (int i = 0; i < m_entries.size(); ++i) {
        const PathEntry& entry = m_entries.at(i);
        EntryState& state = m_states[static_cast<std::size_t>(i)];
        if (entry.keys.isEmpty() || entryResolved(i) || !ownerIndexInRange(entry)) {
            continue;
        }
        if (entry.pRow->continuous) {
            // Meters and positions change every audio buffer: they are
            // polled, never listened to.
            PollingControlProxy polled(entry.keys.first(), kOptionalControl);
            if (polled.valid()) {
                state.polled.emplace(polled);
            }
            continue;
        }
        state.proxies.reserve(static_cast<std::size_t>(entry.keys.size()));
        for (const ConfigKey& key : entry.keys) {
            auto* pProxy = new ControlProxy(key, this, kOptionalControl);
            if (!pProxy->valid()) {
                delete pProxy;
                continue;
            }
            pProxy->connectValueChanged(this, &MmccAdapter::slotDiscreteControlChanged);
            state.proxies.push_back(pProxy);
        }
        if (state.proxies.size() != static_cast<std::size_t>(entry.keys.size())) {
            // All or nothing, so a later call can try again.
            for (ControlProxy* pProxy : state.proxies) {
                delete pProxy;
            }
            state.proxies.clear();
        }
    }
}

bool MmccAdapter::entryResolved(int entryIndex) const {
    const EntryState& state = m_states[static_cast<std::size_t>(entryIndex)];
    return state.polled.has_value() || !state.proxies.empty();
}

int MmccAdapter::numDecks() const {
    if (!m_pNumDecks || !m_pNumDecks->valid()) {
        return 0;
    }
    // More than 4 decks are treated as 4.
    return qBound(0, static_cast<int>(m_pNumDecks->get()), kNumDecks);
}

int MmccAdapter::numSamplers() const {
    if (!m_pNumSamplers || !m_pNumSamplers->valid()) {
        return 0;
    }
    return qBound(0, static_cast<int>(m_pNumSamplers->get()), kNumSamplers);
}

bool MmccAdapter::deckAvailable(int deck) const {
    return deck >= 0 && deck < numDecks() && m_deckLoadedEntry[deck] >= 0 &&
            entryResolved(m_deckLoadedEntry[deck]);
}

bool MmccAdapter::samplerAvailable(int sampler) const {
    return sampler >= 0 && sampler < numSamplers() && m_samplerLoadedEntry[sampler] >= 0 &&
            entryResolved(m_samplerLoadedEntry[sampler]);
}

bool MmccAdapter::deckLoaded(int deck) const {
    // The control and the track are set by different threads at slightly
    // different times. A deck counts as loaded only when both agree, so that
    // `loaded`, the title and the position never contradict each other.
    return deckAvailable(deck) && readControl(m_deckLoadedEntry[deck]) > 0.0 &&
            m_deckTracks[deck];
}

bool MmccAdapter::samplerLoaded(int sampler) const {
    return samplerAvailable(sampler) && readControl(m_samplerLoadedEntry[sampler]) > 0.0;
}

int MmccAdapter::stemCount(int deck) const {
    if (!deckLoaded(deck) || m_deckStemCountEntry[deck] < 0) {
        return 0;
    }
    const double count = readControl(m_deckStemCountEntry[deck]);
    if (!util_isfinite(count)) {
        return 0;
    }
    return qBound(0, static_cast<int>(count), kStemsPerDeck);
}

bool MmccAdapter::ownerAvailable(const PathEntry& entry) const {
    switch (entry.pRow->owner) {
    case Owner::Deck:
        return deckAvailable(entry.index);
    case Owner::Stem:
        // Mixxx creates all four stem controls per deck whatever is loaded.
        // A stem the loaded track does not have addresses no audio: it is
        // null and stale_object, decided here from stem_count.
        return entry.subIndex < stemCount(entry.index);
    case Owner::Sampler:
        return samplerAvailable(entry.index);
    case Owner::Adapter:
    case Owner::Global:
    case Owner::EffectUnit:
        break;
    }
    return true;
}

bool MmccAdapter::ownerEmpty(const PathEntry& entry) const {
    switch (entry.pRow->owner) {
    case Owner::Deck:
    case Owner::Stem:
        return !deckLoaded(entry.index);
    case Owner::Sampler:
        return !samplerLoaded(entry.index);
    case Owner::Adapter:
    case Owner::Global:
    case Owner::EffectUnit:
        break;
    }
    return false;
}

double MmccAdapter::readControl(int entryIndex) const {
    const PathEntry& entry = m_entries.at(entryIndex);
    const EntryState& state = m_states[static_cast<std::size_t>(entryIndex)];
    double value = 0.0;
    if (state.polled) {
        value = state.polled->get();
    } else if (entry.pRow->source == Source::AnyControlValue) {
        for (const ControlProxy* pProxy : state.proxies) {
            if (pProxy->get() > 0.0) {
                value = 1.0;
            }
        }
    } else if (!state.proxies.empty()) {
        const ControlProxy* pProxy = state.proxies.front();
        value = entry.pRow->source == Source::ControlParameter
                ? pProxy->getParameter()
                : pProxy->get();
    }
    return util_isfinite(value) ? value : 0.0;
}

QJsonValue MmccAdapter::computeValue(int entryIndex) const {
    const PathEntry& entry = m_entries.at(entryIndex);
    const PathRow& row = *entry.pRow;
    if (row.source == Source::Constant) {
        return kStatusConnected;
    }
    if (!ownerAvailable(entry)) {
        return QJsonValue::Null;
    }
    if (ownerEmpty(entry)) {
        switch (row.whenEmpty) {
        case WhenEmpty::Null:
            return QJsonValue::Null;
        case WhenEmpty::Zero:
            return row.type == ValueType::Count ? QJsonValue(0) : QJsonValue(false);
        case WhenEmpty::Keep:
            break;
        }
    }
    if (row.source == Source::TrackTitle || row.source == Source::TrackArtist) {
        const TrackPointer& pTrack = m_deckTracks[entry.index];
        if (!pTrack) {
            return QJsonValue::Null;
        }
        return cutTag(row.source == Source::TrackTitle ? pTrack->getTitle()
                                                       : pTrack->getArtist());
    }
    const double raw = readControl(entryIndex);
    switch (row.type) {
    case ValueType::Bool:
        return raw > 0.0;
    case ValueType::Unit:
        return quantise(qBound(0.0, raw, 1.0), row.decimals);
    case ValueType::Positive: {
        const double rounded = quantise(raw, row.decimals);
        return rounded > 0.0 ? QJsonValue(rounded) : QJsonValue(QJsonValue::Null);
    }
    case ValueType::Bipolar:
    case ValueType::Number:
        return quantise(raw, row.decimals);
    case ValueType::Count:
        return qBound(0, static_cast<int>(raw), kStemsPerDeck);
    case ValueType::Status:
    case ValueType::Text:
        break;
    }
    return QJsonValue::Null;
}

QJsonObject MmccAdapter::snapshotValues() const {
    // The values last reported, never a fresh reading: two snapshots at the
    // same revision are always identical.
    QJsonObject values;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).pRow->readable) {
            values.insert(m_entries.at(i).path, m_states[static_cast<std::size_t>(i)].reported);
        }
    }
    return values;
}

// --- Commands ---

MmccAdapter::CommandOutcome MmccAdapter::execute(int entryIndex, const QJsonValue& value) {
    const PathEntry& entry = m_entries.at(entryIndex);
    const PathRow& row = *entry.pRow;
    VERIFY_OR_DEBUG_ASSERT(row.writable) {
        return {QStringLiteral("unsupported_path"), QJsonValue()};
    }

    // The value is judged before the object. Booleans are not numbers and
    // numbers are not booleans; nothing is clamped or rounded into range.
    double target = 0.0;
    if (row.type == ValueType::Bool) {
        if (!value.isBool()) {
            return {QStringLiteral("invalid_value"), QJsonValue()};
        }
        target = value.toBool() ? 1.0 : 0.0;
    } else {
        const double minimum = row.type == ValueType::Bipolar ? -1.0 : 0.0;
        if (!value.isDouble() || !util_isfinite(value.toDouble()) ||
                value.toDouble() < minimum || value.toDouble() > 1.0) {
            return {QStringLiteral("invalid_value"), QJsonValue()};
        }
        target = quantise(value.toDouble(), row.decimals);
    }

    EntryState& state = m_states[static_cast<std::size_t>(entryIndex)];
    if (!ownerAvailable(entry) || state.proxies.empty()) {
        return {QStringLiteral("stale_object"), QJsonValue()};
    }
    if (row.conflictWhenEmpty && ownerEmpty(entry)) {
        return {QStringLiteral("conflict"), QJsonValue()};
    }

    // A command that asks for the value the control already reports applies
    // nothing: re-sending a command changes nothing, and a cue button that
    // is already down is not pressed again.
    if (readBack(entryIndex) != target) {
        ControlProxy* pProxy = state.proxies.front();
        if (row.source == Source::ControlParameter) {
            pProxy->setParameter(target);
        } else {
            pProxy->set(target);
        }
        // Write, then read back. Mixxx may refuse a value for reasons that
        // cannot be seen in advance (a play request, for one).
        if (readBack(entryIndex) != target) {
            if (row.appliedByEngine) {
                // Only a request was filed; the engine thread answers it in
                // its next callback. The caller waits for that, not here.
                CommandOutcome outcome;
                outcome.pending = true;
                outcome.target = target;
                return outcome;
            }
            return {QStringLiteral("conflict"), QJsonValue()};
        }
    }
    return {QString(), effectiveValue(entryIndex, target)};
}

double MmccAdapter::readBack(int entryIndex) const {
    const PathRow& row = *m_entries.at(entryIndex).pRow;
    const double raw = readControl(entryIndex);
    if (row.type == ValueType::Bool) {
        return raw > 0.0 ? 1.0 : 0.0;
    }
    return quantise(raw, row.decimals);
}

bool MmccAdapter::hasTaken(int entryIndex, double target) const {
    return readBack(entryIndex) == target;
}

QJsonValue MmccAdapter::effectiveValue(int entryIndex, double target) const {
    if (m_entries.at(entryIndex).pRow->type == ValueType::Bool) {
        return QJsonValue(target > 0.0);
    }
    return QJsonValue(target);
}

// --- Change reporting ---

void MmccAdapter::slotDiscreteControlChanged() {
    scheduleDiscreteFlush();
}

void MmccAdapter::slotCountsChanged() {
    ensureProxies();
    scheduleDiscreteFlush();
}

void MmccAdapter::slotTrackChanged(
        const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pOldTrack);
    const int deck = deckIndexForGroup(group);
    if (deck < 0) {
        return;
    }
    if (m_deckTracks[deck]) {
        disconnect(m_deckTracks[deck].get(), nullptr, this, nullptr);
    }
    m_deckTracks[deck] = pNewTrack;
    if (pNewTrack) {
        // An edit of the loaded track's tags is an ordinary discrete change.
        connect(pNewTrack.get(),
                &Track::titleChanged,
                this,
                &MmccAdapter::slotDiscreteControlChanged);
        connect(pNewTrack.get(),
                &Track::artistChanged,
                this,
                &MmccAdapter::slotDiscreteControlChanged);
    }
    scheduleDiscreteFlush();
}

void MmccAdapter::scheduleDiscreteFlush() {
    // Control changes that belong together (a load moves a dozen controls)
    // arrive as separate signals. They are gathered until the event loop
    // comes round, which is at once, and reported under one revision. This
    // never waits for the meter timer.
    if (m_flushScheduled) {
        return;
    }
    m_flushScheduled = true;
    QMetaObject::invokeMethod(this, &MmccAdapter::flushDiscrete, Qt::QueuedConnection);
}

void MmccAdapter::flushDiscrete() {
    m_flushScheduled = false;
    // A command that waits for the engine is answered before the change it
    // caused is reported: the result always comes before the event.
    const QList<MmccSession*> waiting = m_sessions;
    for (MmccSession* pSession : waiting) {
        pSession->resolvePending(false);
    }
    QList<int> changed;
    changed.reserve(static_cast<qsizetype>(m_entries.size()));
    for (int i = 0; i < m_entries.size(); ++i) {
        const PathRow& row = *m_entries.at(i).pRow;
        if (!row.readable) {
            continue;
        }
        EntryState& state = m_states[static_cast<std::size_t>(i)];
        QJsonValue current = computeValue(i);
        if (row.continuous) {
            // A continuous path is reported here only when it moves between
            // null and a value, which is part of a load, an eject or a deck
            // appearing. Its level belongs to the meter frames.
            if (current.isNull() == state.reported.isNull()) {
                continue;
            }
        } else if (current == state.reported) {
            continue;
        }
        state.reported = current;
        changed.append(i);
    }
    publish(changed);
}

void MmccAdapter::pollContinuous() {
    // Discrete state first, so that a meter frame never carries a path that
    // changed between null and a value.
    flushDiscrete();

    QList<int> changed;
    changed.reserve(kNumDecks * 3);
    QList<QJsonValue> values;
    values.reserve(kNumDecks * 3);
    for (int i = 0; i < m_entries.size(); ++i) {
        const PathRow& row = *m_entries.at(i).pRow;
        if (!row.continuous) {
            continue;
        }
        const QJsonValue current = computeValue(i);
        if (current != m_states[static_cast<std::size_t>(i)].reported) {
            changed.append(i);
            values.append(current);
        }
    }
    if (changed.isEmpty()) {
        return;
    }
    // The rate gate. A sample that is refused here is not kept anywhere: the
    // next tick reads the control again, so the newest sample always wins,
    // nothing is queued and a stall never turns into a backlog.
    const qint64 now = m_config.clockNs();
    if (m_lastMeterEmitNs && now - *m_lastMeterEmitNs < m_meterIntervalNs) {
        return;
    }
    m_lastMeterEmitNs = now;
    for (qsizetype k = 0; k < changed.size(); ++k) {
        m_states[static_cast<std::size_t>(changed.at(k))].reported = values.at(k);
    }
    publish(changed);
}

void MmccAdapter::publish(const QList<int>& changedEntries) {
    if (changedEntries.isEmpty()) {
        return;
    }
    // One state change advances the revision once.
    ++m_revision;
    if (m_sessions.isEmpty()) {
        return;
    }
    QJsonObject values;
    for (int i : changedEntries) {
        values.insert(m_entries.at(i).path, m_states[static_cast<std::size_t>(i)].reported);
    }
    // A session that cannot keep up closes itself while being served, which
    // changes m_sessions: iterate over a copy.
    const QList<MmccSession*> sessions = m_sessions;
    for (MmccSession* pSession : sessions) {
        if (pSession->isOpen() && pSession->helloReceived()) {
            pSession->sendEvents(m_revision, changedEntries, values);
        }
    }
}

// --- Sessions ---

void MmccAdapter::slotNewConnection() {
    while (QTcpSocket* pSocket = m_pServer->nextPendingConnection()) {
        // Closed sessions were reclaimed when they closed; every session
        // counted here is live, and a live session is never evicted.
        if (m_sessions.size() >= m_config.maxSessions) {
            kLogger.warning() << "Refusing a connection: session limit reached";
            pSocket->abort();
            delete pSocket;
            continue;
        }
        pSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        m_sessions.append(new MmccSession(this, pSocket));
    }
}

void MmccAdapter::sessionClosed(MmccSession* pSession) {
    if (m_sessions.removeAll(pSession) > 0) {
        pSession->deleteLater();
    }
}

void MmccAdapter::noteOutboundQueueSize(int size) {
    m_peakOutboundQueue = qMax(m_peakOutboundQueue, size);
}

} // namespace mmcc
