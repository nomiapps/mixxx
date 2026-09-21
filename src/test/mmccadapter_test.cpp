#include "mmcc/mmccadapter.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QThread>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "control/controlaudiotaperpot.h"
#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlpushbutton.h"
#include "mixer/playerinfo.h"
#include "mmcc/mmccpathtable.h"
#include "mmcc/mmccsession.h"
#include "test/mixxxtest.h"
#include "track/track.h"

// Tests of the MMCC adapter against the contract docs/mixxx-adapter-paths.md
// (MMCC repository). The server is driven by a QTcpSocket client inside the
// test process; both ends live on the main thread, so every wait pumps the
// event loop.

namespace {

using mmcc::MmccAdapter;
using mmcc::MmccAdapterConfig;

constexpr int kTimeoutMs = 5000;
constexpr qint64 kGeneration = 7;

void pumpOnce() {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

bool pumpUntil(const std::function<bool()>& done, int timeoutMs = kTimeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        pumpOnce();
        if (done()) {
            break;
        }
        QThread::msleep(1);
    }
    return true;
}

void pumpFor(int ms) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        pumpOnce();
        QThread::msleep(1);
    }
}

QString deckGroup(int deck) {
    return QStringLiteral("[Channel%1]").arg(deck + 1);
}

/// A raw NDJSON client.
class TestClient {
  public:
    explicit TestClient(quint16 port) {
        m_socket.connectToHost(QHostAddress::LocalHost, port);
        pumpUntil([this]() { return m_socket.state() == QAbstractSocket::ConnectedState; });
        // Let the server accept (or refuse) the connection.
        pumpFor(20);
    }

    QTcpSocket& socket() {
        return m_socket;
    }

    void sendRaw(const QByteArray& bytes) {
        m_socket.write(bytes);
        m_socket.flush();
    }

    qint64 send(QJsonObject message, std::optional<qint64> sequence = std::nullopt) {
        if (!message.contains(QStringLiteral("protocol"))) {
            message.insert(QStringLiteral("protocol"),
                    QJsonObject{{QStringLiteral("major"), 1}, {QStringLiteral("minor"), 0}});
        }
        message.insert(QStringLiteral("client_id"), QStringLiteral("mmcc-test"));
        const qint64 used = sequence ? *sequence : m_sequence++;
        if (!message.contains(QStringLiteral("sequence"))) {
            message.insert(QStringLiteral("sequence"), used);
        }
        sendRaw(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
        return used;
    }

    /// The next frame, or an empty object after the timeout. Checks that the
    /// frames are numbered 0, 1, 2, ... in arrival order.
    QJsonObject next(int timeoutMs = kTimeoutMs) {
        if (!pumpUntil([this]() { return m_socket.canReadLine(); }, timeoutMs)) {
            return QJsonObject();
        }
        const QJsonObject frame = QJsonDocument::fromJson(m_socket.readLine()).object();
        EXPECT_EQ(m_nextInbound, frame.value(QStringLiteral("sequence")).toInteger(-1));
        m_nextInbound = frame.value(QStringLiteral("sequence")).toInteger(-1) + 1;
        EXPECT_EQ(kGeneration, frame.value(QStringLiteral("generation")).toInteger(-1));
        return frame;
    }

    /// The next frame of `kind`; the frames before it are kept in `skipped`.
    QJsonObject nextOfKind(const QString& kind) {
        for (;;) {
            const QJsonObject frame = next();
            if (frame.isEmpty() || frame.value(QStringLiteral("kind")).toString() == kind) {
                return frame;
            }
            skipped.append(frame);
        }
    }

    /// Everything that arrives until the connection has been quiet.
    QList<QJsonObject> drain(int quietMs = 60) {
        QList<QJsonObject> frames;
        frames.reserve(16);
        for (;;) {
            const QJsonObject frame = next(quietMs);
            if (frame.isEmpty()) {
                return frames;
            }
            frames.append(frame);
        }
    }

    /// hello; returns the snapshot's values.
    QJsonObject hello() {
        send({{QStringLiteral("kind"), QStringLiteral("hello")},
                {QStringLiteral("role"), QStringLiteral("test")},
                {QStringLiteral("name"), QStringLiteral("test")},
                {QStringLiteral("instance_id"), QStringLiteral("test-1")}});
        helloFrame = nextOfKind(QStringLiteral("hello"));
        capabilities = nextOfKind(QStringLiteral("capabilities"));
        snapshot = nextOfKind(QStringLiteral("snapshot"));
        return snapshot.value(QStringLiteral("values")).toObject();
    }

    QJsonObject subscribe(const QString& id, const QStringList& paths) {
        send({{QStringLiteral("kind"), QStringLiteral("subscribe")},
                {QStringLiteral("subscription_id"), id},
                {QStringLiteral("paths"), QJsonArray::fromStringList(paths)}});
        return nextOfKind(QStringLiteral("snapshot"));
    }

    void subscribeAll(const QStringList& paths) {
        for (int first = 0; first < paths.size(); first += 16) {
            const QJsonObject reply =
                    subscribe(QStringLiteral("all-%1").arg(first / 16), paths.mid(first, 16));
            ASSERT_FALSE(reply.isEmpty());
        }
    }

    /// Sends one command and returns its result. Events that arrive before
    /// the result are kept in `skipped`.
    QJsonObject command(const QString& path,
            const QJsonValue& value,
            const QString& id = QString(),
            std::optional<qint64> sequence = std::nullopt,
            const QString& operation = QStringLiteral("set"),
            bool omitValue = false) {
        QJsonObject message{{QStringLiteral("kind"), QStringLiteral("command")},
                {QStringLiteral("command_id"),
                        id.isEmpty() ? QStringLiteral("cmd-%1").arg(m_sequence) : id},
                {QStringLiteral("adapter"), QStringLiteral("mixxx")},
                {QStringLiteral("path"), path},
                {QStringLiteral("operation"), operation}};
        if (!omitValue) {
            message.insert(QStringLiteral("value"), value);
        }
        send(message, sequence);
        return nextOfKind(QStringLiteral("result"));
    }

    bool waitDisconnected(int timeoutMs = kTimeoutMs) {
        return pumpUntil(
                [this]() { return m_socket.state() == QAbstractSocket::UnconnectedState; },
                timeoutMs);
    }

    qint64 nextSequence() const {
        return m_sequence;
    }

    QJsonObject helloFrame;
    QJsonObject capabilities;
    QJsonObject snapshot;
    QList<QJsonObject> skipped;

  private:
    QTcpSocket m_socket;
    qint64 m_sequence = 0;
    qint64 m_nextInbound = 0;
};

QString errorCode(const QJsonObject& result) {
    EXPECT_FALSE(result.value(QStringLiteral("ok")).toBool(true));
    const QJsonObject error = result.value(QStringLiteral("error")).toObject();
    EXPECT_FALSE(error.value(QStringLiteral("message")).toString().isEmpty());
    return error.value(QStringLiteral("code")).toString();
}

/// The controls of a Mixxx with four deck groups, the way the engine, the
/// effects and the players create them, without any of those.
class FakeMixxx {
  public:
    FakeMixxx(int numDecks, int numSamplers) {
        add<ControlObject>(ConfigKey("[App]", "num_decks"))->set(numDecks);
        add<ControlObject>(ConfigKey("[App]", "num_samplers"))->set(numSamplers);
        add<ControlObject>(ConfigKey("[App]", "num_preview_decks"));
        add<ControlPotmeter>(ConfigKey("[Master]", "crossfader"), -1.0, 1.0);
        add<ControlAudioTaperPot>(ConfigKey("[Master]", "gain"), -14, 14, 0.5);
        for (int deck = 0; deck < mmcc::kNumDecks; ++deck) {
            const QString group = deckGroup(deck);
            add<ControlAudioTaperPot>(ConfigKey(group, "volume"), -20, 0, 1);
            add<ControlPushButton>(ConfigKey(group, "mute"));
            auto* pLoaded = add<ControlObject>(ConfigKey(group, "track_loaded"));
            auto* pPlay = add<ControlPushButton>(ConfigKey(group, "play"));
            // The engine refuses to play an empty deck, and may refuse for
            // reasons the adapter cannot see (`refusePlay`).
            pPlay->connectValueChangeRequest(
                    pPlay,
                    [this, pPlay, pLoaded](double value) {
                        if (value > 0.0 && (pLoaded->get() <= 0.0 || refusePlay)) {
                            pPlay->setAndConfirm(0.0);
                        } else {
                            pPlay->setAndConfirm(value);
                        }
                    },
                    Qt::DirectConnection);
            add<ControlPushButton>(ConfigKey(group, "cue_default"));
            add<ControlObject>(ConfigKey(group, "cue_indicator"));
            // Like the real one, sync_enabled only files a request that the
            // engine thread applies in its next callback.
            auto* pSync = add<ControlPushButton>(ConfigKey(group, "sync_enabled"));
            pSync->connectValueChangeRequest(
                    pSync,
                    [this, pSync](double value) {
                        m_syncRequests.emplace_back(pSync, value);
                        if (engineRunning) {
                            runEngineCallback();
                        }
                    },
                    Qt::DirectConnection);
            add<ControlPushButton>(ConfigKey(group, "pfl"));
            add<ControlObject>(ConfigKey(group, "bpm"));
            add<ControlPotmeter>(ConfigKey(group, "rate"), -1.0, 1.0, true);
            add<ControlObject>(ConfigKey(group, "stem_count"));
            add<ControlObject>(ConfigKey(group, "vu_meter_left"));
            add<ControlObject>(ConfigKey(group, "vu_meter_right"));
            add<ControlObject>(ConfigKey(group, "playposition"));
            // PlayerInfo reads these two.
            add<ControlObject>(ConfigKey(group, "pregain"));
            add<ControlObject>(ConfigKey(group, "orientation"));
            add<ControlPotmeter>(
                    ConfigKey(QStringLiteral("[QuickEffectRack1_%1]").arg(group), "super1"))
                    ->set(0.5);
            for (int stem = 0; stem < mmcc::kStemsPerDeck; ++stem) {
                const QString stemGroup =
                        QStringLiteral("[Channel%1_Stem%2]").arg(deck + 1).arg(stem + 1);
                add<ControlPotmeter>(ConfigKey(stemGroup, "volume"))->set(1.0);
                add<ControlPushButton>(ConfigKey(stemGroup, "mute"));
            }
        }
        for (int unit = 0; unit < mmcc::kNumEffectUnits; ++unit) {
            const QString group = QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit + 1);
            add<ControlPotmeter>(ConfigKey(group, "mix"));
            add<ControlPushButton>(ConfigKey(group, "enabled"));
            add<ControlPotmeter>(ConfigKey(group, "super1"));
            for (int slot = 0; slot < mmcc::kEffectSlotsPerUnit; ++slot) {
                add<ControlObject>(ConfigKey(
                        QStringLiteral("[EffectRack1_EffectUnit%1_Effect%2]")
                                .arg(unit + 1)
                                .arg(slot + 1),
                        "loaded"));
            }
        }
        for (int sampler = 0; sampler < mmcc::kNumSamplers; ++sampler) {
            const QString group = QStringLiteral("[Sampler%1]").arg(sampler + 1);
            add<ControlObject>(ConfigKey(group, "track_loaded"));
            add<ControlPushButton>(ConfigKey(group, "play"));
            add<ControlAudioTaperPot>(ConfigKey(group, "volume"), -20, 0, 1);
        }
    }

    static void set(const QString& group, const QString& key, double value) {
        ControlObject::set(ConfigKey(group, key), value);
    }
    static double get(const QString& group, const QString& key) {
        return ControlObject::get(ConfigKey(group, key));
    }

    /// What the engine and the player do when a track has been loaded.
    TrackPointer load(int deck,
            const QString& title,
            const QString& artist,
            double bpm,
            int stems) {
        const QString group = deckGroup(deck);
        set(group, "stem_count", stems);
        set(group, "bpm", bpm);
        set(group, "playposition", 0.0);
        set(group, "cue_indicator", 1.0);
        set(group, "track_loaded", 1.0);
        TrackPointer pTrack = Track::newTemporary();
        pTrack->setTitle(title);
        pTrack->setArtist(artist);
        PlayerInfo::instance().setTrackInfo(group, pTrack);
        return pTrack;
    }

    void eject(int deck) {
        const QString group = deckGroup(deck);
        set(group, "play", 0.0);
        set(group, "track_loaded", 0.0);
        set(group, "stem_count", 0.0);
        set(group, "bpm", 0.0);
        set(group, "cue_indicator", 0.0);
        PlayerInfo::instance().setTrackInfo(group, TrackPointer());
    }

    void clear() {
        m_syncRequests.clear();
        m_controls.clear();
    }

    /// What the engine does with the filed requests in an audio callback.
    void runEngineCallback() {
        for (const auto& [pControl, value] : m_syncRequests) {
            pControl->setAndConfirm(value);
        }
        m_syncRequests.clear();
    }

    bool refusePlay = false;
    /// False models the time between two audio callbacks, or no audio device.
    bool engineRunning = true;

  private:
    template<typename T, typename... Args>
    T* add(Args&&... args) {
        auto pControl = std::make_unique<T>(std::forward<Args>(args)...);
        T* pRaw = pControl.get();
        m_controls.push_back(std::move(pControl));
        return pRaw;
    }

    std::vector<std::unique_ptr<ControlObject>> m_controls;
    std::vector<std::pair<ControlPushButton*, double>> m_syncRequests;
};

class MmccAdapterTest : public MixxxTest {
  protected:
    ~MmccAdapterTest() override {
        m_pAdapter.reset();
        PlayerInfo::destroy();
        m_pMixxx.reset();
    }

    /// Deck 0 holds a stem track, deck 1 an ordinary track, deck 2 is empty
    /// and deck 3 does not exist; sampler 0 is loaded, sampler 1 empty, the
    /// others do not exist; effect unit 0 has an effect.
    void startMixxx(int numDecks = 3, int numSamplers = 2) {
        m_pMixxx = std::make_unique<FakeMixxx>(numDecks, numSamplers);
        // PlayerInfo reads [App] and [Master] controls, so it comes second.
        PlayerInfo::create();
        m_pMixxx->load(0, QStringLiteral("Stem Track"), QStringLiteral("Artist A"), 124.0, 4);
        if (numDecks > 1) {
            m_pMixxx->load(1, QStringLiteral("Track"), QStringLiteral("Artist B"), 128.504, 0);
        }
        FakeMixxx::set("[Sampler1]", "track_loaded", 1.0);
        FakeMixxx::set("[EffectRack1_EffectUnit1_Effect2]", "loaded", 1.0);
    }

    MmccAdapterConfig testConfig() {
        MmccAdapterConfig config;
        config.port = 0;
        config.generation = kGeneration;
        config.startMeterTimer = false;
        config.clockNs = [this]() { return m_nowNs; };
        return config;
    }

    void startAdapter(const MmccAdapterConfig& config) {
        m_pAdapter = std::make_unique<MmccAdapter>(config, &PlayerInfo::instance());
        ASSERT_TRUE(m_pAdapter->isListening());
    }

    void start() {
        startMixxx();
        startAdapter(testConfig());
    }

    quint16 port() const {
        return m_pAdapter->serverPort();
    }

    std::unique_ptr<FakeMixxx> m_pMixxx;
    std::unique_ptr<MmccAdapter> m_pAdapter;
    qint64 m_nowNs = 1000000000;
};

QStringList continuousPaths() {
    QStringList paths;
    paths.reserve(mmcc::kNumDecks * 3);
    const QList<mmcc::PathEntry> entries = mmcc::buildPathTable();
    for (const mmcc::PathEntry& entry : entries) {
        if (entry.pRow->continuous) {
            paths.append(entry.path);
        }
    }
    return paths;
}

// --- The path table ---

TEST_F(MmccAdapterTest, PathTableMatchesTheContractCounts) {
    const QList<mmcc::PathEntry> entries = mmcc::buildPathTable();
    QStringList readable;
    QStringList writable;
    readable.reserve(entries.size());
    writable.reserve(entries.size());
    for (const mmcc::PathEntry& entry : entries) {
        if (entry.pRow->readable) {
            readable.append(entry.path);
        }
        if (entry.pRow->writable) {
            writable.append(entry.path);
            // No continuous path is ever writable.
            EXPECT_FALSE(entry.pRow->continuous) << qPrintable(entry.path);
        }
    }
    EXPECT_EQ(139, readable.size());
    EXPECT_EQ(94, writable.size());
    EXPECT_EQ(readable.size(), QSet<QString>(readable.begin(), readable.end()).size());
    EXPECT_EQ(12, continuousPaths().size());

    // Globals, then the four decks: the first 55 paths.
    EXPECT_QSTRING_EQ("surface.connection.status", readable.at(0));
    EXPECT_QSTRING_EQ("mixxx.crossfader", readable.at(1));
    EXPECT_QSTRING_EQ("mixxx.main.gain", readable.at(2));
    EXPECT_QSTRING_EQ("channel[0].name", readable.at(3));
    EXPECT_QSTRING_EQ("mixxx.deck[0].quick_effect", readable.at(15));
    EXPECT_QSTRING_EQ("channel[1].name", readable.at(16));
    EXPECT_QSTRING_EQ("mixxx.deck[3].quick_effect", readable.at(54));
    // Meters and positions, deck-major.
    EXPECT_QSTRING_EQ("channel[0].meter.left", readable.at(55));
    EXPECT_QSTRING_EQ("channel[0].meter.right", readable.at(56));
    EXPECT_QSTRING_EQ("mixxx.deck[0].position", readable.at(57));
    // Stems, effect units, samplers.
    EXPECT_QSTRING_EQ("mixxx.deck[0].stem[0].volume", readable.at(67));
    EXPECT_QSTRING_EQ("mixxx.deck[0].stem[0].mute", readable.at(68));
    EXPECT_QSTRING_EQ("mixxx.deck[0].stem[1].volume", readable.at(69));
    EXPECT_QSTRING_EQ("mixxx.effect_unit[0].mix", readable.at(99));
    EXPECT_QSTRING_EQ("mixxx.sampler[0].loaded", readable.at(115));
    EXPECT_QSTRING_EQ("mixxx.sampler[7].volume", readable.at(138));

    // The cue button is write-only, its light read-only.
    EXPECT_FALSE(readable.contains(QStringLiteral("mixxx.deck[0].cue")));
    EXPECT_TRUE(writable.contains(QStringLiteral("mixxx.deck[0].cue")));
    EXPECT_FALSE(writable.contains(QStringLiteral("mixxx.deck[0].cue_indicator")));
    // There is no global transport.
    EXPECT_FALSE(readable.join(' ').contains(QStringLiteral("transport.")));
}

TEST_F(MmccAdapterTest, PathTableGroupsCountFromOne) {
    const QList<mmcc::PathEntry> entries = mmcc::buildPathTable();
    QHash<QString, ConfigKey> keys;
    keys.reserve(entries.size());
    for (const mmcc::PathEntry& entry : entries) {
        if (entry.keys.size() == 1) {
            keys.insert(entry.path, entry.keys.first());
        }
    }
    EXPECT_EQ(ConfigKey("[Master]", "crossfader"), keys.value("mixxx.crossfader"));
    EXPECT_EQ(ConfigKey("[Channel1]", "volume"), keys.value("channel[0].volume"));
    EXPECT_EQ(ConfigKey("[Channel4]", "cue_default"), keys.value("mixxx.deck[3].cue"));
    EXPECT_EQ(ConfigKey("[QuickEffectRack1_[Channel2]]", "super1"),
            keys.value("mixxx.deck[1].quick_effect"));
    EXPECT_EQ(ConfigKey("[Channel3_Stem4]", "mute"), keys.value("mixxx.deck[2].stem[3].mute"));
    EXPECT_EQ(ConfigKey("[EffectRack1_EffectUnit4]", "super1"),
            keys.value("mixxx.effect_unit[3].super"));
    EXPECT_EQ(ConfigKey("[Sampler8]", "play"), keys.value("mixxx.sampler[7].play"));
}

TEST_F(MmccAdapterTest, QuantiseRoundsHalvesAwayFromZero) {
    EXPECT_DOUBLE_EQ(0.124, mmcc::quantise(0.1236, 3));
    EXPECT_DOUBLE_EQ(0.125, mmcc::quantise(0.1245, 3));
    EXPECT_DOUBLE_EQ(-0.1235, mmcc::quantise(-0.12346, 4));
    EXPECT_DOUBLE_EQ(-0.5, mmcc::quantise(-0.45, 1));
    EXPECT_DOUBLE_EQ(128.5, mmcc::quantise(128.504, 2));
    EXPECT_FALSE(std::signbit(mmcc::quantise(-0.0001, 3)));
}

// --- Sessions ---

TEST_F(MmccAdapterTest, HelloIsAnsweredWithHelloCapabilitiesAndAFullSnapshot) {
    start();
    TestClient client(port());
    const QJsonObject values = client.hello();

    EXPECT_EQ(0, client.helloFrame.value("sequence").toInteger(-1));
    EXPECT_QSTRING_EQ("application_adapter", client.helloFrame.value("role").toString());
    EXPECT_EQ(1, client.capabilities.value("sequence").toInteger(-1));
    EXPECT_EQ(2, client.snapshot.value("sequence").toInteger(-1));
    EXPECT_TRUE(client.skipped.isEmpty());

    QStringList readable;
    const QJsonArray readableArray = client.capabilities.value("readable_paths").toArray();
    readable.reserve(readableArray.size());
    for (const QJsonValue& path : readableArray) {
        readable.append(path.toString());
    }
    EXPECT_EQ(m_pAdapter->readablePaths(), readable);
    EXPECT_EQ(94, client.capabilities.value("writable_paths").toArray().size());
    EXPECT_EQ(24, client.capabilities.value("meter_rate_hz").toInt());
    EXPECT_QSTRING_EQ("mixxx", client.snapshot.value("adapter").toString());

    // The snapshot covers exactly the readable paths.
    const QStringList valueKeys = values.keys();
    EXPECT_EQ(QSet<QString>(readable.begin(), readable.end()),
            QSet<QString>(valueKeys.begin(), valueKeys.end()));

    // hello again on the same session is answered the same way.
    const QJsonObject again = client.hello();
    EXPECT_EQ(values, again);
    EXPECT_EQ(5, client.snapshot.value("sequence").toInteger(-1));
}

TEST_F(MmccAdapterTest, SnapshotFollowsTheNullRules) {
    start();
    FakeMixxx::set("[Channel1]", "volume", 0.5);
    FakeMixxx::set("[Master]", "crossfader", -0.25);
    TestClient client(port());
    const QJsonObject values = client.hello();

    EXPECT_QSTRING_EQ("connected", values.value("surface.connection.status").toString());
    EXPECT_DOUBLE_EQ(-0.25, values.value("mixxx.crossfader").toDouble());
    // An audio-taper gain is its parameter: unity is 0.5 on the main gain
    // and 1.0 on a deck fader.
    EXPECT_DOUBLE_EQ(0.5, values.value("mixxx.main.gain").toDouble());
    EXPECT_DOUBLE_EQ(1.0, values.value("channel[1].volume").toDouble());
    const double halfGainParameter =
            ControlObject::getControl(ConfigKey("[Channel1]", "volume"))->getParameter();
    EXPECT_DOUBLE_EQ(mmcc::quantise(halfGainParameter, 3),
            values.value("channel[0].volume").toDouble());
    EXPECT_NE(0.5, values.value("channel[0].volume").toDouble());

    // Deck 0: a stem track.
    EXPECT_QSTRING_EQ("Stem Track", values.value("channel[0].name").toString());
    EXPECT_QSTRING_EQ("Artist A", values.value("mixxx.deck[0].artist").toString());
    EXPECT_TRUE(values.value("mixxx.deck[0].loaded").toBool());
    EXPECT_EQ(4, values.value("mixxx.deck[0].stem_count").toInt());
    EXPECT_DOUBLE_EQ(1.0, values.value("mixxx.deck[0].stem[3].volume").toDouble());
    EXPECT_TRUE(values.value("mixxx.deck[0].stem[3].mute").isBool());
    EXPECT_DOUBLE_EQ(0.0, values.value("mixxx.deck[0].position").toDouble());
    EXPECT_TRUE(values.value("mixxx.deck[0].position").isDouble());

    // Deck 1: an ordinary track has no stems, although Mixxx created the
    // stem controls.
    EXPECT_DOUBLE_EQ(128.5, values.value("mixxx.deck[1].bpm").toDouble());
    EXPECT_EQ(0, values.value("mixxx.deck[1].stem_count").toInt(-1));
    EXPECT_TRUE(values.value("mixxx.deck[1].stem[0].volume").isNull());
    EXPECT_TRUE(values.value("mixxx.deck[1].stem[0].mute").isNull());

    // Deck 2: empty but available. The track's paths are null, the deck's
    // paths keep their values.
    EXPECT_TRUE(values.value("channel[2].name").isNull());
    EXPECT_TRUE(values.value("mixxx.deck[2].artist").isNull());
    EXPECT_TRUE(values.value("mixxx.deck[2].bpm").isNull());
    EXPECT_TRUE(values.value("mixxx.deck[2].position").isNull());
    EXPECT_TRUE(values.value("mixxx.deck[2].loaded").isBool());
    EXPECT_FALSE(values.value("mixxx.deck[2].loaded").toBool(true));
    EXPECT_FALSE(values.value("mixxx.deck[2].play").toBool(true));
    EXPECT_FALSE(values.value("mixxx.deck[2].cue_indicator").toBool(true));
    EXPECT_EQ(0, values.value("mixxx.deck[2].stem_count").toInt(-1));
    EXPECT_TRUE(values.value("mixxx.deck[2].stem[0].volume").isNull());
    EXPECT_DOUBLE_EQ(1.0, values.value("channel[2].volume").toDouble());
    EXPECT_DOUBLE_EQ(0.5, values.value("mixxx.deck[2].quick_effect").toDouble());
    EXPECT_DOUBLE_EQ(0.0, values.value("channel[2].meter.left").toDouble(-1));
    EXPECT_TRUE(values.value("mixxx.deck[2].sync").isBool());

    // Deck 3: n >= num_decks, every path is null.
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (it.key().startsWith("channel[3].") || it.key().startsWith("mixxx.deck[3].")) {
            EXPECT_TRUE(it.value().isNull()) << qPrintable(it.key());
        }
    }

    // Effect units are never null; `loaded` is any of the four slots.
    EXPECT_TRUE(values.value("mixxx.effect_unit[0].loaded").toBool());
    EXPECT_TRUE(values.value("mixxx.effect_unit[1].loaded").isBool());
    EXPECT_FALSE(values.value("mixxx.effect_unit[1].loaded").toBool(true));
    EXPECT_TRUE(values.value("mixxx.effect_unit[3].mix").isDouble());
    EXPECT_TRUE(values.value("mixxx.effect_unit[3].enabled").isBool());

    // Samplers: 0 loaded, 1 empty, 2.. unavailable.
    EXPECT_TRUE(values.value("mixxx.sampler[0].loaded").toBool());
    EXPECT_FALSE(values.value("mixxx.sampler[1].loaded").toBool(true));
    EXPECT_DOUBLE_EQ(1.0, values.value("mixxx.sampler[1].volume").toDouble());
    EXPECT_TRUE(values.value("mixxx.sampler[2].loaded").isNull());
    EXPECT_TRUE(values.value("mixxx.sampler[2].play").isNull());
    EXPECT_TRUE(values.value("mixxx.sampler[7].volume").isNull());
}

TEST_F(MmccAdapterTest, AnythingBeforeHelloClosesWithoutAReply) {
    start();
    FakeMixxx::set("[Channel1]", "mute", 0.0);
    const QList<QJsonObject> firstMessages = {
            {{"kind", "heartbeat"}, {"monotonic_ms", 0}},
            {{"kind", "subscribe"},
                    {"subscription_id", "s"},
                    {"paths", QJsonArray{"channel[0].mute"}}},
            {{"kind", "command"},
                    {"command_id", "c"},
                    {"adapter", "mixxx"},
                    {"path", "channel[0].mute"},
                    {"operation", "set"},
                    {"value", true}},
    };
    for (const QJsonObject& message : firstMessages) {
        TestClient client(port());
        client.send(message);
        EXPECT_TRUE(client.waitDisconnected());
        EXPECT_FALSE(client.socket().canReadLine());
    }
    // No command was applied.
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));
    EXPECT_EQ(0, m_pAdapter->sessionCount());
}

TEST_F(MmccAdapterTest, ProtocolErrorsCloseTheConnection) {
    start();
    const QString longId(129, QChar('x'));
    const QList<QByteArray> badLines = {
            "not json\n",
            "[1,2,3]\n",
            R"({"protocol":{"major":1.0,"minor":0},"kind":"heartbeat","sequence":1,"monotonic_ms":0})"
            "\n",
            R"({"protocol":{"major":true,"minor":0},"kind":"heartbeat","sequence":1,"monotonic_ms":0})"
            "\n",
            R"({"protocol":{"major":2,"minor":0},"kind":"heartbeat","sequence":1,"monotonic_ms":0})"
            "\n",
            R"({"protocol":{"major":1,"minor":0},"kind":"heartbeat","sequence":1,"monotonic_ms":-1})"
            "\n",
            R"({"protocol":{"major":1,"minor":0},"kind":"heartbeat","sequence":1,"monotonic_ms":1.5})"
            "\n",
            R"({"protocol":{"major":1,"minor":0},"kind":"heartbeat","sequence":1,"monotonic_ms":NaN})"
            "\n",
            R"({"protocol":{"major":1,"minor":0},"kind":"focus","sequence":1})"
            "\n",
            R"({"protocol":{"major":1,"minor":0},"kind":"command","sequence":1,"path":"channel[0].mute","operation":"set","value":true})"
            "\n",
            QByteArray(R"({"protocol":{"major":1,"minor":0},"kind":"command","sequence":1,"command_id":")") +
                    longId.toUtf8() +
                    R"(","path":"channel[0].mute","operation":"set","value":true})"
                    "\n",
    };
    for (const QByteArray& line : badLines) {
        TestClient client(port());
        client.hello();
        client.sendRaw(line);
        EXPECT_TRUE(client.waitDisconnected()) << line.constData();
        EXPECT_FALSE(client.socket().canReadLine()) << line.constData();
    }
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));

    // A heartbeat is echoed, and a 128 character identifier is fine.
    TestClient client(port());
    client.hello();
    client.send({{"kind", "heartbeat"}, {"monotonic_ms", Q_INT64_C(1234567890123)}});
    const QJsonObject heartbeat = client.nextOfKind("heartbeat");
    EXPECT_EQ(Q_INT64_C(1234567890123), heartbeat.value("monotonic_ms").toInteger());
    const QJsonObject result =
            client.command("channel[0].mute", true, QString(128, QChar('y')));
    EXPECT_TRUE(result.value("ok").toBool());
}

TEST_F(MmccAdapterTest, ALineOverTheLimitClosesTheConnection) {
    startMixxx();
    MmccAdapterConfig config = testConfig();
    config.maxLineBytes = 4096;
    startAdapter(config);

    // Exactly at the limit is accepted: unknown fields are ignored.
    {
        TestClient client(port());
        QByteArray line =
                R"({"protocol":{"major":1,"minor":0},"kind":"hello","sequence":0,"pad":")";
        line.append(QByteArray(4096 - line.size() - 2, 'a'));
        line.append("\"}");
        ASSERT_EQ(4096, line.size());
        client.sendRaw(line + '\n');
        EXPECT_FALSE(client.nextOfKind("snapshot").isEmpty());
    }
    // One byte more is not, with or without a newline in sight.
    {
        TestClient client(port());
        client.hello();
        client.sendRaw(QByteArray(4097, 'a') + '\n');
        EXPECT_TRUE(client.waitDisconnected());
    }
    {
        TestClient client(port());
        client.hello();
        client.sendRaw(QByteArray(64 * 1024, 'a'));
        EXPECT_TRUE(client.waitDisconnected());
    }
    EXPECT_EQ(0, m_pAdapter->sessionCount());
}

// --- Commands ---

TEST_F(MmccAdapterTest, EveryWritablePathTakesALegalValueAndRejectsWrongOnes) {
    start();
    TestClient client(port());
    client.hello();
    client.subscribeAll(m_pAdapter->readablePaths());
    QJsonObject values = client.snapshot.value("values").toObject();

    int written = 0;
    int stale = 0;
    int conflicts = 0;
    QList<QJsonValue> illegal;
    for (const QString& path : m_pAdapter->writablePaths()) {
        SCOPED_TRACE(qPrintable(path));
        const mmcc::PathEntry& entry =
                m_pAdapter->entries().at(m_pAdapter->writableIndex(path));
        const mmcc::PathRow& row = *entry.pRow;
        const bool isBool = row.type == mmcc::ValueType::Bool;
        const bool isBipolar = row.type == mmcc::ValueType::Bipolar;

        // Wrong values are judged before the object: invalid_value in every
        // state, and nothing changes.
        if (isBool) {
            illegal = {1, QStringLiteral("true"), QJsonValue::Null};
        } else if (isBipolar) {
            illegal = {1.00004, -1.5, true, QStringLiteral("0.5")};
        } else {
            illegal = {1.0004, -0.001, true, QJsonValue::Null};
        }
        const qint64 revisionBefore = m_pAdapter->revision();
        for (const QJsonValue& bad : std::as_const(illegal)) {
            EXPECT_QSTRING_EQ("invalid_value", errorCode(client.command(path, bad)));
        }
        EXPECT_EQ(revisionBefore, m_pAdapter->revision());

        QJsonValue send;
        QJsonValue effective;
        if (isBool) {
            send = effective = !values.value(path).toBool(false);
        } else if (isBipolar) {
            send = -0.12346;
            effective = -0.1235;
        } else {
            send = 0.1236;
            effective = 0.124;
        }

        // What decides the answer.
        const bool deckPath = row.owner == mmcc::Owner::Deck;
        const bool stemPath = row.owner == mmcc::Owner::Stem;
        const bool samplerPath = row.owner == mmcc::Owner::Sampler;
        bool expectStale = false;
        bool expectConflict = false;
        if (deckPath) {
            expectStale = entry.index >= 3;
            expectConflict = !expectStale && row.conflictWhenEmpty && entry.index == 2;
        } else if (stemPath) {
            expectStale = entry.index != 0;
        } else if (samplerPath) {
            expectStale = entry.index >= 2;
            expectConflict = !expectStale && row.conflictWhenEmpty && entry.index == 1;
        }

        client.skipped.clear();
        const QJsonObject result = client.command(path, send);
        if (expectStale) {
            EXPECT_QSTRING_EQ("stale_object", errorCode(result));
            ++stale;
            continue;
        }
        if (expectConflict) {
            EXPECT_QSTRING_EQ("conflict", errorCode(result));
            ++conflicts;
            continue;
        }
        ++written;
        ASSERT_TRUE(result.value("ok").toBool()) << qPrintable(errorCode(result));
        EXPECT_EQ(effective, result.value("effective_value"));
        // The result comes before any event that reports its effect.
        EXPECT_TRUE(client.skipped.isEmpty());

        // The control really moved, in the space the contract names.
        const ControlObject* pControl = ControlObject::getControl(entry.keys.first());
        const double raw = row.source == mmcc::Source::ControlParameter
                ? pControl->getParameter()
                : pControl->get();
        if (isBool) {
            EXPECT_EQ(effective.toBool(), raw > 0.0);
        } else {
            EXPECT_NEAR(effective.toDouble(), raw, 1e-9);
        }

        if (row.readable) {
            const QJsonObject event = client.nextOfKind("event");
            ASSERT_FALSE(event.isEmpty());
            EXPECT_EQ(effective, event.value("changes").toObject().value(path));
            EXPECT_EQ(m_pAdapter->revision(), event.value("revision").toInteger());
        }
        // Re-sending the same command succeeds and changes nothing.
        const qint64 revisionAfter = m_pAdapter->revision();
        const QJsonObject again = client.command(path, send);
        EXPECT_TRUE(again.value("ok").toBool());
        EXPECT_EQ(effective, again.value("effective_value"));
        EXPECT_EQ(revisionAfter, m_pAdapter->revision());
        EXPECT_TRUE(client.drain(5).isEmpty());
        if (path.endsWith(".cue") || path.endsWith(".play")) {
            client.command(path, false);
            client.drain(5);
        }
        values = m_pAdapter->snapshotValues();
    }
    // 2 globals, 3 decks x 8 less play and cue of the empty deck, 8 stems of
    // deck 0, 12 effect unit paths, 2 samplers x 2 less play of the empty one.
    EXPECT_EQ(2 + 24 - 2 + 8 + 12 + 4 - 1, written);
    EXPECT_EQ(8 + 24 + 12, stale);
    EXPECT_EQ(3, conflicts);
}

TEST_F(MmccAdapterTest, RefusedCommandsGetOneCodeInTheContractsOrder) {
    start();
    TestClient client(port());
    client.hello();
    client.subscribe("s", {"mixxx.crossfader", "mixxx.deck[1].play"});
    const qint64 revision = m_pAdapter->revision();

    struct Case {
        QString path;
        QJsonValue value;
        QString operation;
        bool omitValue;
        QString code;
    };
    const QList<Case> cases = {
            {"transport.play", true, "set", false, "unsupported_path"},
            {"channel[4].volume", 0.5, "set", false, "unsupported_path"},
            {"channel.bank", 0, "set", false, "unsupported_path"},
            {"mixxx.deck[0].bpm", 120.0, "set", false, "unsupported_path"},
            {"mixxx.deck[0].position", 0.5, "set", false, "unsupported_path"},
            {"channel[0].meter.left", 0.5, "set", false, "unsupported_path"},
            {"mixxx.deck[0].cue_indicator", true, "set", false, "unsupported_path"},
            {"channel[0].name", "x", "set", false, "unsupported_path"},
            {QString(600, QChar('p')), 0.5, "set", false, "unsupported_path"},
            {QStringLiteral("mixxx.cross") + QChar(1) + QStringLiteral("fader"),
                    0.5,
                    "set",
                    false,
                    "unsupported_path"},
            {"mixxx.crossfader", 1, "invoke", false, "unsupported_path"},
            // The operation is judged before the path, a missing value after it.
            {"transport.play", 0.0, "frobnicate", false, "invalid_command"},
            {"mixxx.crossfader", 0.0, "frobnicate", false, "invalid_command"},
            {"mixxx.crossfader", QJsonValue(), "set", true, "invalid_command"},
            {"transport.play", QJsonValue(), "set", true, "unsupported_path"},
            // The value is judged before the object.
            {"channel[3].volume", 2.0, "set", false, "invalid_value"},
            {"channel[3].volume", 0.5, "set", false, "stale_object"},
            {"mixxx.deck[1].stem[0].volume", 7, "set", false, "invalid_value"},
            {"mixxx.deck[1].stem[0].volume", 0.5, "set", false, "stale_object"},
            {"mixxx.deck[2].play", 1, "set", false, "invalid_value"},
            {"mixxx.deck[2].play", true, "set", false, "conflict"},
            {"mixxx.deck[2].cue", true, "set", false, "conflict"},
            {"mixxx.deck[3].play", true, "set", false, "stale_object"},
            {"mixxx.sampler[1].play", true, "set", false, "conflict"},
            {"mixxx.sampler[2].play", true, "set", false, "stale_object"},
    };
    for (const Case& c : cases) {
        const QJsonObject result =
                client.command(c.path, c.value, QString(), std::nullopt, c.operation, c.omitValue);
        EXPECT_QSTRING_EQ(c.code, errorCode(result)) << qPrintable(c.path);
    }

    // Read-back: the engine refuses the play request although a track is
    // loaded. The result is conflict and no change is reported.
    m_pMixxx->refusePlay = true;
    EXPECT_QSTRING_EQ("conflict", errorCode(client.command("mixxx.deck[1].play", true)));
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel2]", "play"));
    m_pMixxx->refusePlay = false;

    // A failed command changes no state and reports nothing.
    EXPECT_TRUE(client.drain().isEmpty());
    EXPECT_EQ(revision, m_pAdapter->revision());
    EXPECT_TRUE(client.command("mixxx.deck[1].play", true).value("ok").toBool());
    EXPECT_TRUE(client.nextOfKind("event")
                    .value("changes")
                    .toObject()
                    .value("mixxx.deck[1].play")
                    .toBool());
}

TEST_F(MmccAdapterTest, CuePressIsWriteOnlyAndItsEffectsFollowTheResult) {
    start();
    // Pressing cue on a playing deck stops it, as one cue mode of Mixxx does.
    ControlObject* pCue = ControlObject::getControl(ConfigKey("[Channel1]", "cue_default"));
    int presses = 0;
    QObject context;
    QObject::connect(pCue, &ControlObject::valueChanged, &context, [&presses](double value) {
        if (value > 0.0) {
            ++presses;
            FakeMixxx::set("[Channel1]", "play", 0.0);
        }
    });
    TestClient client(port());
    client.hello();
    client.subscribe("s", {"mixxx.deck[0].play", "mixxx.deck[0].cue_indicator"});
    ASSERT_TRUE(client.command("mixxx.deck[0].play", true).value("ok").toBool());
    ASSERT_FALSE(client.nextOfKind("event").isEmpty());

    client.skipped.clear();
    const QJsonObject result = client.command("mixxx.deck[0].cue", true);
    EXPECT_TRUE(result.value("ok").toBool());
    EXPECT_EQ(QJsonValue(true), result.value("effective_value"));
    EXPECT_TRUE(client.skipped.isEmpty());
    const QJsonObject event = client.nextOfKind("event");
    EXPECT_EQ(QJsonValue(false), event.value("changes").toObject().value("mixxx.deck[0].play"));
    EXPECT_FALSE(event.value("changes").toObject().contains("mixxx.deck[0].cue"));

    // Setting the state it already has succeeds and presses nothing.
    EXPECT_TRUE(client.command("mixxx.deck[0].cue", true).value("ok").toBool());
    EXPECT_EQ(1, presses);
    const QJsonObject release = client.command("mixxx.deck[0].cue", false);
    EXPECT_EQ(QJsonValue(false), release.value("effective_value"));
    EXPECT_DOUBLE_EQ(0.0, pCue->get());
    EXPECT_TRUE(client.drain().isEmpty());
}

TEST_F(MmccAdapterTest, ReplayReturnsTheRetainedResultAndAppliesNothing) {
    start();
    TestClient client(port());
    client.hello();

    const qint64 firstSequence = client.nextSequence();
    const QJsonObject first = client.command("channel[0].mute", true, "replay-a");
    ASSERT_TRUE(first.value("ok").toBool());
    EXPECT_DOUBLE_EQ(1.0, FakeMixxx::get("[Channel1]", "mute"));
    // Mixxx moves the control back; a replay must not touch it again.
    FakeMixxx::set("[Channel1]", "mute", 0.0);

    // The identical frame: same ID, content and (now stale) sequence.
    QJsonObject again = client.command("channel[0].mute", true, "replay-a", firstSequence);
    EXPECT_TRUE(again.value("ok").toBool());
    EXPECT_EQ(first.value("effective_value"), again.value("effective_value"));
    EXPECT_GT(again.value("sequence").toInteger(), first.value("sequence").toInteger());
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));

    // The same ID and content with a fresh sequence is still a replay.
    again = client.command("channel[0].mute", true, "replay-a");
    EXPECT_TRUE(again.value("ok").toBool());
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));

    // The same ID with different content. true is not 1, absent is not null,
    // another path or operation is other content.
    EXPECT_QSTRING_EQ("invalid_command",
            errorCode(client.command("channel[0].mute", false, "replay-a")));
    EXPECT_QSTRING_EQ("invalid_command",
            errorCode(client.command("channel[0].mute", 1, "replay-a")));
    EXPECT_QSTRING_EQ("invalid_command",
            errorCode(client.command("channel[1].mute", true, "replay-a")));
    EXPECT_QSTRING_EQ("invalid_command",
            errorCode(client.command("channel[0].mute", true, "replay-a", std::nullopt, "invoke")));
    EXPECT_QSTRING_EQ("invalid_command",
            errorCode(client.command(
                    "channel[0].mute", QJsonValue(), "replay-a", std::nullopt, "set", true)));
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel2]", "mute"));

    // A new ID with a sequence at or below the watermark.
    EXPECT_QSTRING_EQ("invalid_command",
            errorCode(client.command("channel[0].mute", true, "replay-c", firstSequence)));
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));

    // Failures are retained too: the replay of a refused command is the same
    // refusal even after the state that caused it has gone.
    EXPECT_QSTRING_EQ("conflict",
            errorCode(client.command("mixxx.deck[2].play", true, "replay-d")));
    m_pMixxx->load(2, "Late", "Artist", 100.0, 0);
    EXPECT_QSTRING_EQ("conflict",
            errorCode(client.command("mixxx.deck[2].play", true, "replay-d")));
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel3]", "play"));
}

TEST_F(MmccAdapterTest, GenerationAndSequenceAreJudgedFirst) {
    start();
    TestClient client(port());
    client.hello();
    const auto commandWith = [&client](const QString& key, const QJsonValue& value) {
        QJsonObject message{{"kind", "command"},
                {"command_id", QStringLiteral("gen-%1").arg(client.nextSequence())},
                {"adapter", "mixxx"},
                {"path", "channel[0].mute"},
                {"operation", "set"},
                {"value", true}};
        message.insert(key, value);
        client.send(message);
        return client.nextOfKind("result");
    };
    EXPECT_QSTRING_EQ("invalid_command", errorCode(commandWith("generation", kGeneration + 1)));
    EXPECT_QSTRING_EQ("invalid_command", errorCode(commandWith("generation", true)));
    EXPECT_QSTRING_EQ("invalid_command", errorCode(commandWith("generation", 7.5)));
    EXPECT_QSTRING_EQ("invalid_command", errorCode(commandWith("sequence", -1)));
    EXPECT_QSTRING_EQ("invalid_command", errorCode(commandWith("sequence", 3.5)));
    EXPECT_QSTRING_EQ("invalid_command", errorCode(commandWith("sequence", "4")));
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));
    // The adapter's own generation is accepted.
    EXPECT_TRUE(commandWith("generation", kGeneration).value("ok").toBool());
    EXPECT_DOUBLE_EQ(1.0, FakeMixxx::get("[Channel1]", "mute"));
}

TEST_F(MmccAdapterTest, TheResultCacheReclaimsAtItsCapAndNeverRevivesAStaleId) {
    startMixxx();
    MmccAdapterConfig config = testConfig();
    config.maxCommandsPerSession = 4;
    startAdapter(config);
    TestClient client(port());
    client.hello();
    const auto* pSession = m_pAdapter->findChild<mmcc::MmccSession*>();
    ASSERT_NE(nullptr, pSession);

    // Exactly at the cap every entry replays.
    QList<qint64> sequences;
    sequences.reserve(4);
    for (int i = 0; i < 4; ++i) {
        sequences.append(client.nextSequence());
        ASSERT_TRUE(client.command("channel[0].mute", i % 2 == 0, QStringLiteral("cap-%1").arg(i))
                        .value("ok")
                        .toBool());
    }
    EXPECT_EQ(4, pSession->commandCacheSize());
    const double muteBefore = FakeMixxx::get("[Channel1]", "mute");
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(client.command("channel[0].mute",
                                  i % 2 == 0,
                                  QStringLiteral("cap-%1").arg(i),
                                  sequences.at(i))
                        .value("ok")
                        .toBool());
    }
    EXPECT_DOUBLE_EQ(muteBefore, FakeMixxx::get("[Channel1]", "mute"));

    // One more evicts only the oldest.
    ASSERT_TRUE(client.command("channel[0].mute", true, "cap-4").value("ok").toBool());
    EXPECT_EQ(4, pSession->commandCacheSize());
    for (int i = 1; i < 4; ++i) {
        // Still retained: different content is a reused ID, not a new command.
        EXPECT_QSTRING_EQ("invalid_command",
                errorCode(client.command(
                        "mixxx.deck[0].pfl", true, QStringLiteral("cap-%1").arg(i))));
    }
    // The evicted ID with its old sequence is stale, not revived...
    FakeMixxx::set("[Channel1]", "mute", 0.0);
    EXPECT_QSTRING_EQ("invalid_command",
            errorCode(client.command("channel[0].mute", true, "cap-0", sequences.at(0))));
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));
    // ...and with a fresh sequence it is a new command.
    EXPECT_TRUE(client.command("channel[0].mute", true, "cap-0").value("ok").toBool());
    EXPECT_DOUBLE_EQ(1.0, FakeMixxx::get("[Channel1]", "mute"));

    // Churn far past the cap: the cache stays within its bound and new work
    // is still accepted.
    for (int i = 0; i < 60; ++i) {
        const QJsonObject churn = client.command("mixxx.deck[0].pfl", i % 2 == 0);
        ASSERT_TRUE(churn.value("ok").toBool())
                << QJsonDocument(churn).toJson(QJsonDocument::Compact).constData();
        ASSERT_LE(pSession->commandCacheSize(), 4);
    }
    EXPECT_EQ(4, pSession->commandCacheSize());
}

TEST_F(MmccAdapterTest, AControlTheEngineAppliesIsReadBackWithoutBlocking) {
    startMixxx();
    MmccAdapterConfig config = testConfig();
    config.engineApplyTimeoutMs = 150;
    startAdapter(config);
    TestClient client(port());
    client.hello();
    client.subscribe("s", {"mixxx.deck[0].sync", "channel[0].mute"});
    TestClient other(port());
    other.hello();
    const auto kinds = [](const QList<QJsonObject>& frames) {
        QStringList list;
        list.reserve(frames.size());
        for (const QJsonObject& frame : frames) {
            list.append(frame.value("kind").toString());
        }
        return list.join(' ');
    };

    // Between two audio callbacks the control still reads the old value. The
    // command is neither answered nor refused yet, and nothing blocks: the
    // other session is served meanwhile.
    m_pMixxx->engineRunning = false;
    const QJsonObject command{{"kind", "command"},
            {"adapter", "mixxx"},
            {"operation", "set"},
            {"value", true}};
    QJsonObject sync = command;
    sync.insert("command_id", "sync-1");
    sync.insert("path", "mixxx.deck[0].sync");
    client.send(sync);
    QJsonObject mute = command;
    mute.insert("command_id", "mute-1");
    mute.insert("path", "channel[0].mute");
    client.send(mute);
    EXPECT_TRUE(client.drain(40).isEmpty());
    EXPECT_DOUBLE_EQ(0.0, FakeMixxx::get("[Channel1]", "mute"));
    EXPECT_TRUE(other.command("mixxx.deck[1].pfl", true).value("ok").toBool());

    // The engine applies the request: result, then its event, then the
    // command that waited behind it, in that order.
    m_pMixxx->runEngineCallback();
    const QList<QJsonObject> frames = client.drain();
    ASSERT_EQ(4, frames.size()) << qPrintable(kinds(frames));
    EXPECT_QSTRING_EQ("result event result event", kinds(frames));
    EXPECT_QSTRING_EQ("sync-1", frames.at(0).value("command_id").toString());
    EXPECT_TRUE(frames.at(0).value("ok").toBool());
    EXPECT_EQ(QJsonValue(true), frames.at(0).value("effective_value"));
    EXPECT_EQ(QJsonValue(true),
            frames.at(1).value("changes").toObject().value("mixxx.deck[0].sync"));
    EXPECT_QSTRING_EQ("mute-1", frames.at(2).value("command_id").toString());
    // The retained result replays like any other.
    EXPECT_TRUE(client.command("mixxx.deck[0].sync", true, "sync-1").value("ok").toBool());

    // The engine never answers (no audio device): conflict after the
    // deadline, no event, and the session carries on.
    const qint64 revision = m_pAdapter->revision();
    EXPECT_QSTRING_EQ("conflict", errorCode(client.command("mixxx.deck[0].sync", false)));
    EXPECT_TRUE(client.drain().isEmpty());
    EXPECT_EQ(revision, m_pAdapter->revision());
    EXPECT_TRUE(client.command("channel[0].mute", false).value("ok").toBool());

    // A session that goes away, and an adapter that goes away, while a
    // command waits.
    sync.insert("command_id", "sync-2");
    sync.insert("value", false);
    m_pMixxx->runEngineCallback();
    client.drain();
    sync.insert("value", true);
    client.send(sync);
    pumpFor(20);
    client.socket().abort();
    ASSERT_TRUE(pumpUntil([this]() { return m_pAdapter->sessionCount() == 1; }));
    sync.insert("path", "mixxx.deck[1].sync");
    other.send(sync);
    pumpFor(20);
    m_pAdapter.reset();
    EXPECT_TRUE(other.waitDisconnected());
    m_pMixxx->runEngineCallback();
}

// --- Subscriptions and events ---

TEST_F(MmccAdapterTest, SubscriptionLimitsAreProtocolErrors) {
    start();
    const QStringList readable = m_pAdapter->readablePaths();
    // 16 paths are fine, duplicates count once, 17 are not.
    {
        TestClient client(port());
        client.hello();
        QStringList paths = readable.mid(0, 16);
        paths.append(readable.at(0));
        EXPECT_FALSE(client.subscribe("s", paths).isEmpty());
        client.send({{"kind", "subscribe"},
                {"subscription_id", "t"},
                {"paths", QJsonArray::fromStringList(readable.mid(0, 17))}});
        EXPECT_TRUE(client.waitDisconnected());
    }
    // An unreadable path, an empty list, a missing id.
    const QList<QJsonObject> bad = {
            {{"kind", "subscribe"},
                    {"subscription_id", "s"},
                    {"paths", QJsonArray{"mixxx.deck[0].cue"}}},
            {{"kind", "subscribe"},
                    {"subscription_id", "s"},
                    {"paths", QJsonArray{"transport.play"}}},
            {{"kind", "subscribe"}, {"subscription_id", "s"}, {"paths", QJsonArray()}},
            {{"kind", "subscribe"}, {"subscription_id", ""}, {"paths", QJsonArray{"mixxx.crossfader"}}},
            {{"kind", "subscribe"}, {"paths", QJsonArray{"mixxx.crossfader"}}},
            {{"kind", "subscribe"}, {"subscription_id", "s"}, {"paths", QJsonArray{5}}},
    };
    for (const QJsonObject& message : bad) {
        TestClient client(port());
        client.hello();
        client.send(message);
        EXPECT_TRUE(client.waitDisconnected());
    }
    // 32 subscriptions: at the cap an existing one can be replaced, a new one
    // is refused.
    {
        TestClient client(port());
        client.hello();
        for (int i = 0; i < 32; ++i) {
            ASSERT_FALSE(
                    client.subscribe(QStringLiteral("s%1").arg(i), {readable.at(i)}).isEmpty());
        }
        const auto* pSession = m_pAdapter->findChild<mmcc::MmccSession*>();
        ASSERT_NE(nullptr, pSession);
        EXPECT_EQ(32, pSession->subscriptionCount());
        EXPECT_FALSE(client.subscribe("s31", {readable.at(40)}).isEmpty());
        EXPECT_EQ(32, pSession->subscriptionCount());
        client.send({{"kind", "subscribe"},
                {"subscription_id", "s32"},
                {"paths", QJsonArray{readable.at(0)}}});
        EXPECT_TRUE(client.waitDisconnected());
    }
    // 256 subscribed paths in total: exactly at the cap is fine, one more is
    // refused, and unsubscribing frees the room again.
    {
        TestClient client(port());
        client.hello();
        for (int i = 0; i < 16; ++i) {
            ASSERT_FALSE(
                    client.subscribe(QStringLiteral("s%1").arg(i), readable.mid(i, 16)).isEmpty());
        }
        client.send({{"kind", "unsubscribe"}, {"subscription_id", "s0"}});
        EXPECT_FALSE(client.subscribe("s0", readable.mid(50, 16)).isEmpty());
        client.send({{"kind", "subscribe"},
                {"subscription_id", "extra"},
                {"paths", QJsonArray{readable.at(0)}}});
        EXPECT_TRUE(client.waitDisconnected());
    }
    EXPECT_EQ(0, m_pAdapter->sessionCount());
}

TEST_F(MmccAdapterTest, ChangesInsideMixxxAreReportedOncePerSubscription) {
    start();
    TestClient client(port());
    client.hello();
    client.subscribe("a", {"mixxx.crossfader", "channel[0].mute"});
    client.subscribe("b", {"mixxx.crossfader"});
    TestClient other(port());
    other.hello();
    other.subscribe("c", {"channel[0].mute"});
    const qint64 revision = m_pAdapter->revision();

    // One change, one revision, one event per subscription that holds it.
    FakeMixxx::set("[Master]", "crossfader", 0.33336);
    const QList<QJsonObject> events = client.drain();
    ASSERT_EQ(2, events.size());
    QSet<QString> ids;
    ids.reserve(events.size());
    for (const QJsonObject& event : events) {
        EXPECT_QSTRING_EQ("event", event.value("kind").toString());
        EXPECT_QSTRING_EQ("mixxx", event.value("adapter").toString());
        EXPECT_EQ(revision + 1, event.value("revision").toInteger());
        const QJsonObject changes = event.value("changes").toObject();
        EXPECT_EQ(1, changes.size());
        EXPECT_DOUBLE_EQ(0.3334, changes.value("mixxx.crossfader").toDouble());
        ids.insert(event.value("subscription_id").toString());
    }
    EXPECT_EQ((QSet<QString>{"a", "b"}), ids);
    EXPECT_TRUE(other.drain().isEmpty());

    // A change that does not change the rounded value is not a change.
    FakeMixxx::set("[Master]", "crossfader", 0.33338);
    EXPECT_TRUE(client.drain().isEmpty());
    EXPECT_EQ(revision + 1, m_pAdapter->revision());

    // A command's effect reaches the other session too, and an unsubscribed
    // subscription gets nothing.
    client.send({{"kind", "unsubscribe"}, {"subscription_id", "a"}});
    ASSERT_TRUE(client.command("channel[0].mute", true).value("ok").toBool());
    EXPECT_TRUE(client.drain().isEmpty());
    const QList<QJsonObject> otherEvents = other.drain();
    ASSERT_EQ(1, otherEvents.size());
    EXPECT_EQ(QJsonValue(true),
            otherEvents.first().value("changes").toObject().value("channel[0].mute"));

    // A snapshot carries the reported state at the current revision.
    const QJsonObject values = client.hello();
    EXPECT_DOUBLE_EQ(0.3334, values.value("mixxx.crossfader").toDouble());
    EXPECT_EQ(m_pAdapter->revision(), client.snapshot.value("revision").toInteger());
}

TEST_F(MmccAdapterTest, LoadAndEjectMoveTheTracksPathsUnderOneRevision) {
    start();
    TestClient client(port());
    client.hello();
    client.subscribeAll(m_pAdapter->readablePaths());
    qint64 revision = m_pAdapter->revision();

    const auto merged = [&client]() {
        QJsonObject all;
        QSet<qint64> revisions;
        const QList<QJsonObject> events = client.drain();
        revisions.reserve(events.size());
        for (const QJsonObject& event : events) {
            revisions.insert(event.value("revision").toInteger());
            const QJsonObject changes = event.value("changes").toObject();
            for (auto it = changes.constBegin(); it != changes.constEnd(); ++it) {
                all.insert(it.key(), it.value());
            }
        }
        EXPECT_LE(revisions.size(), 1);
        return all;
    };

    // The control says loaded before the player has announced the track: the
    // deck does not count as loaded yet, so nothing contradicts itself.
    FakeMixxx::set("[Channel3]", "track_loaded", 1.0);
    EXPECT_TRUE(merged().isEmpty());
    FakeMixxx::set("[Channel3]", "track_loaded", 0.0);

    const QString longTitle(300, QChar('t'));
    TrackPointer pTrack = m_pMixxx->load(2, longTitle, QString(), 99.996, 2);
    QJsonObject changes = merged();
    EXPECT_EQ(revision + 1, m_pAdapter->revision());
    EXPECT_EQ(QJsonValue(true), changes.value("mixxx.deck[2].loaded"));
    // Cut at 256 characters, not rejected; an empty tag is "", not null.
    EXPECT_EQ(256, changes.value("channel[2].name").toString().size());
    EXPECT_TRUE(changes.value("mixxx.deck[2].artist").isString());
    EXPECT_QSTRING_EQ("", changes.value("mixxx.deck[2].artist").toString());
    EXPECT_DOUBLE_EQ(100.0, changes.value("mixxx.deck[2].bpm").toDouble());
    EXPECT_EQ(2, changes.value("mixxx.deck[2].stem_count").toInt());
    EXPECT_EQ(QJsonValue(true), changes.value("mixxx.deck[2].cue_indicator"));
    // The position moves from null to 0.0 as part of the same change.
    EXPECT_TRUE(changes.contains("mixxx.deck[2].position"));
    EXPECT_DOUBLE_EQ(0.0, changes.value("mixxx.deck[2].position").toDouble(-1));
    EXPECT_DOUBLE_EQ(1.0, changes.value("mixxx.deck[2].stem[1].volume").toDouble());
    EXPECT_EQ(QJsonValue(false), changes.value("mixxx.deck[2].stem[1].mute"));
    EXPECT_FALSE(changes.contains("mixxx.deck[2].stem[2].volume"));
    // The deck's own paths did not change and are not reported.
    EXPECT_FALSE(changes.contains("channel[2].volume"));
    EXPECT_FALSE(changes.contains("mixxx.deck[2].play"));
    // A stem the track has now accepts commands, the next one does not.
    EXPECT_TRUE(client.command("mixxx.deck[2].stem[1].mute", true).value("ok").toBool());
    EXPECT_QSTRING_EQ("stale_object",
            errorCode(client.command("mixxx.deck[2].stem[2].mute", true)));
    client.drain();

    // A track without a beat grid reports 0 BPM, which is null.
    FakeMixxx::set("[Channel3]", "bpm", 0.0);
    changes = merged();
    EXPECT_TRUE(changes.contains("mixxx.deck[2].bpm"));
    EXPECT_TRUE(changes.value("mixxx.deck[2].bpm").isNull());

    // An edit of the loaded track's tags is an ordinary discrete change.
    revision = m_pAdapter->revision();
    pTrack->setTitle("Retitled");
    pTrack->setArtist("Someone");
    changes = merged();
    EXPECT_EQ(2, changes.size());
    EXPECT_QSTRING_EQ("Retitled", changes.value("channel[2].name").toString());
    EXPECT_QSTRING_EQ("Someone", changes.value("mixxx.deck[2].artist").toString());
    EXPECT_EQ(revision + 1, m_pAdapter->revision());

    // Eject: the same paths going the other way.
    revision = m_pAdapter->revision();
    m_pMixxx->eject(2);
    changes = merged();
    EXPECT_EQ(revision + 1, m_pAdapter->revision());
    EXPECT_EQ(QJsonValue(false), changes.value("mixxx.deck[2].loaded"));
    for (const QString& path : {QStringLiteral("channel[2].name"),
                 QStringLiteral("mixxx.deck[2].artist"),
                 QStringLiteral("mixxx.deck[2].position"),
                 QStringLiteral("mixxx.deck[2].stem[0].volume"),
                 QStringLiteral("mixxx.deck[2].stem[1].mute")}) {
        EXPECT_TRUE(changes.contains(path)) << qPrintable(path);
        EXPECT_TRUE(changes.value(path).isNull()) << qPrintable(path);
    }
    EXPECT_EQ(0, changes.value("mixxx.deck[2].stem_count").toInt(-1));
    // The old track's tags no longer reach the adapter.
    pTrack->setTitle("Gone");
    EXPECT_TRUE(merged().isEmpty());
}

TEST_F(MmccAdapterTest, ADeckAddedWhileRunningBecomesAvailable) {
    start();
    TestClient client(port());
    QJsonObject values = client.hello();
    EXPECT_TRUE(values.value("channel[3].volume").isNull());
    EXPECT_TRUE(values.value("channel[3].meter.left").isNull());
    client.subscribe("s", {"channel[3].volume", "channel[3].meter.left", "mixxx.deck[3].loaded"});
    EXPECT_QSTRING_EQ("stale_object", errorCode(client.command("channel[3].volume", 0.5)));

    FakeMixxx::set("[App]", "num_decks", 4);
    const QJsonObject event = client.nextOfKind("event");
    const QJsonObject changes = event.value("changes").toObject();
    EXPECT_DOUBLE_EQ(1.0, changes.value("channel[3].volume").toDouble());
    EXPECT_DOUBLE_EQ(0.0, changes.value("channel[3].meter.left").toDouble(-1));
    EXPECT_EQ(QJsonValue(false), changes.value("mixxx.deck[3].loaded"));
    EXPECT_TRUE(client.command("channel[3].volume", 0.5).value("ok").toBool());

    // More than four decks are treated as four.
    FakeMixxx::set("[App]", "num_decks", 6);
    EXPECT_TRUE(client.command("channel[3].volume", 0.25).value("ok").toBool());
}

// --- Meters and position ---

TEST_F(MmccAdapterTest, MeterFramesAreRateLimitedAndCoalesced) {
    start();
    TestClient client(port());
    client.hello();
    client.subscribe("m", continuousPaths());
    const qint64 intervalNs = 1000000000 / 24;
    qint64 revision = m_pAdapter->revision();

    // The first frame goes out at once.
    FakeMixxx::set("[Channel1]", "vu_meter_left", 0.50049);
    m_pAdapter->pollContinuous();
    QList<QJsonObject> frames = client.drain();
    ASSERT_EQ(1, frames.size());
    EXPECT_DOUBLE_EQ(0.5,
            frames.first().value("changes").toObject().value("channel[0].meter.left").toDouble());
    EXPECT_EQ(++revision, frames.first().value("revision").toInteger());

    // Samples taken inside the interval are held, not queued.
    const QList<double> levels = {0.6, 0.7, 0.8};
    for (const double level : levels) {
        m_nowNs += intervalNs / 4;
        FakeMixxx::set("[Channel1]", "vu_meter_left", level);
        FakeMixxx::set("[Channel1]", "playposition", level / 2);
        m_pAdapter->pollContinuous();
    }
    EXPECT_TRUE(client.drain().isEmpty());
    EXPECT_EQ(revision, m_pAdapter->revision());
    // A held sample is not yet state: a snapshot reports the emitted value.
    EXPECT_DOUBLE_EQ(0.5, client.hello().value("channel[0].meter.left").toDouble());

    // One nanosecond before the interval has passed: still nothing.
    m_nowNs += intervalNs / 4 - 1;
    m_pAdapter->pollContinuous();
    EXPECT_TRUE(client.drain().isEmpty());

    // At the interval exactly one frame carries the newest samples only.
    m_nowNs += 1 + intervalNs % 4;
    m_pAdapter->pollContinuous();
    frames = client.drain();
    ASSERT_EQ(1, frames.size());
    const QJsonObject changes = frames.first().value("changes").toObject();
    EXPECT_EQ(2, changes.size());
    EXPECT_DOUBLE_EQ(0.8, changes.value("channel[0].meter.left").toDouble());
    EXPECT_DOUBLE_EQ(0.4, changes.value("mixxx.deck[0].position").toDouble());
    EXPECT_EQ(++revision, frames.first().value("revision").toInteger());

    // No change, no frame and no revision, however much time passes.
    m_nowNs += 10 * intervalNs;
    m_pAdapter->pollContinuous();
    EXPECT_TRUE(client.drain().isEmpty());
    EXPECT_EQ(revision, m_pAdapter->revision());

    // A long burst never exceeds the rate: 1000 polls over one second.
    int emitted = 0;
    for (int i = 0; i < 1000; ++i) {
        m_nowNs += 1000000;
        FakeMixxx::set("[Channel2]", "vu_meter_right", (i % 100) / 100.0);
        m_pAdapter->pollContinuous();
        if (i % 50 == 49) {
            emitted += static_cast<int>(client.drain(0).size());
        }
    }
    emitted += static_cast<int>(client.drain().size());
    EXPECT_LE(emitted, 24);
    EXPECT_GE(emitted, 20);

    // The rate is bounded and validated; a rejected rate leaves the old one.
    EXPECT_FALSE(m_pAdapter->setMeterRateHz(0));
    EXPECT_FALSE(m_pAdapter->setMeterRateHz(61));
    EXPECT_EQ(24, m_pAdapter->meterRateHz());
    EXPECT_TRUE(m_pAdapter->setMeterRateHz(60));
    EXPECT_EQ(60, m_pAdapter->meterRateHz());
}

TEST_F(MmccAdapterTest, ADiscreteChangeNeverWaitsForTheMeterGate) {
    start();
    TestClient client(port());
    client.hello();
    QStringList paths = continuousPaths();
    paths.append("channel[0].mute");
    client.subscribe("s", paths);

    FakeMixxx::set("[Channel1]", "vu_meter_left", 0.5);
    m_pAdapter->pollContinuous();
    ASSERT_EQ(1, client.drain().size());

    // The gate is closed and a meter sample is waiting.
    m_nowNs += 1000;
    FakeMixxx::set("[Channel1]", "vu_meter_left", 0.9);
    m_pAdapter->pollContinuous();
    const qint64 revision = m_pAdapter->revision();

    // The mute goes out at once, in its own frame, without the clock moving
    // and without a poll. The waiting meter sample is not swept along.
    FakeMixxx::set("[Channel1]", "mute", 1.0);
    const QList<QJsonObject> frames = client.drain();
    ASSERT_EQ(1, frames.size());
    const QJsonObject changes = frames.first().value("changes").toObject();
    EXPECT_EQ(1, changes.size());
    EXPECT_EQ(QJsonValue(true), changes.value("channel[0].mute"));
    EXPECT_EQ(revision + 1, frames.first().value("revision").toInteger());

    // An eject while a position sample waits: the event carries the position
    // going to null, and the old sample is never reported afterwards.
    FakeMixxx::set("[Channel1]", "playposition", 0.75);
    m_pAdapter->pollContinuous();
    m_pMixxx->eject(0);
    const QList<QJsonObject> ejectFrames = client.drain();
    ASSERT_EQ(1, ejectFrames.size());
    EXPECT_TRUE(ejectFrames.first().value("changes").toObject().contains("mixxx.deck[0].position"));
    EXPECT_TRUE(ejectFrames.first()
                    .value("changes")
                    .toObject()
                    .value("mixxx.deck[0].position")
                    .isNull());
    m_nowNs += 1000000000;
    m_pAdapter->pollContinuous();
    const QList<QJsonObject> meterFrames = client.drain();
    ASSERT_EQ(1, meterFrames.size());
    // The held meter sample survived the eject, the position did not.
    EXPECT_EQ(QStringList{"channel[0].meter.left"},
            meterFrames.first().value("changes").toObject().keys());
}

TEST_F(MmccAdapterTest, TheMeterTimerRunsByItself) {
    startMixxx();
    MmccAdapterConfig config = testConfig();
    config.startMeterTimer = true;
    config.meterRateHz = 60;
    config.clockNs = nullptr;
    startAdapter(config);
    TestClient client(port());
    client.hello();
    EXPECT_EQ(60, client.capabilities.value("meter_rate_hz").toInt());
    client.subscribe("m", continuousPaths());
    FakeMixxx::set("[Channel1]", "vu_meter_right", 0.25);
    const QJsonObject event = client.nextOfKind("event");
    EXPECT_DOUBLE_EQ(0.25,
            event.value("changes").toObject().value("channel[0].meter.right").toDouble());
}

// --- Bounds ---

TEST_F(MmccAdapterTest, APeerThatStopsReadingIsDisconnectedNotBuffered) {
    startMixxx();
    MmccAdapterConfig config = testConfig();
    config.maxOutboundQueueSize = 8;
    config.socketHighWaterBytes = 4096;
    startAdapter(config);

    TestClient slow(port());
    slow.hello();
    slow.subscribe("s", {"mixxx.crossfader"});
    // From here on the peer reads nothing.
    slow.socket().setReadBufferSize(1024);
    const QByteArray hello =
            R"({"protocol":{"major":1,"minor":0},"kind":"hello","client_id":"slow","sequence":1,"role":"test","name":"slow","instance_id":"slow-1"})"
            "\n";
    int sent = 0;
    // The last condition only ends the loop when the cap is broken, so that a
    // broken cap fails this test instead of eating memory.
    while (m_pAdapter->sessionCount() > 0 && sent < 200000 &&
            m_pAdapter->peakOutboundQueueSize() <= 8) {
        for (int i = 0; i < 50; ++i) {
            slow.socket().write(hello);
            ++sent;
        }
        slow.socket().flush();
        pumpOnce();
    }
    EXPECT_EQ(0, m_pAdapter->sessionCount()) << "after " << sent << " hellos";
    // The queue reached its cap and never went past it.
    EXPECT_EQ(8, m_pAdapter->peakOutboundQueueSize());
    EXPECT_TRUE(slow.waitDisconnected());

    // The adapter still serves others.
    TestClient client(port());
    EXPECT_FALSE(client.hello().isEmpty());
    EXPECT_TRUE(client.command("channel[0].mute", true).value("ok").toBool());
}

TEST_F(MmccAdapterTest, SessionsAreCappedAndALiveSessionIsNeverEvicted) {
    startMixxx();
    MmccAdapterConfig config = testConfig();
    config.maxSessions = 2;
    startAdapter(config);

    auto pFirst = std::make_unique<TestClient>(port());
    auto pSecond = std::make_unique<TestClient>(port());
    ASSERT_FALSE(pFirst->hello().isEmpty());
    ASSERT_FALSE(pSecond->hello().isEmpty());
    EXPECT_EQ(2, m_pAdapter->sessionCount());

    // Exactly at the cap one more is refused, and every held one still works.
    {
        TestClient third(port());
        EXPECT_TRUE(third.waitDisconnected());
        EXPECT_FALSE(third.socket().canReadLine());
    }
    EXPECT_EQ(2, m_pAdapter->sessionCount());
    EXPECT_TRUE(pFirst->command("channel[0].mute", true).value("ok").toBool());
    EXPECT_TRUE(pSecond->command("mixxx.deck[0].pfl", true).value("ok").toBool());

    // Freeing one slot admits exactly one.
    pSecond.reset();
    ASSERT_TRUE(pumpUntil([this]() { return m_pAdapter->sessionCount() == 1; }));
    TestClient fourth(port());
    EXPECT_FALSE(fourth.hello().isEmpty());
    TestClient fifth(port());
    EXPECT_TRUE(fifth.waitDisconnected());

    // Churn: many short connections never use up the slots, and the long
    // lived session survives them.
    for (int i = 0; i < 40; ++i) {
        fourth.socket().abort();
        ASSERT_TRUE(pumpUntil([this]() { return m_pAdapter->sessionCount() == 1; }));
        TestClient shortLived(port());
        ASSERT_FALSE(shortLived.hello().isEmpty()) << i;
        shortLived.socket().abort();
        ASSERT_TRUE(pumpUntil([this]() { return m_pAdapter->sessionCount() == 1; }));
    }
    EXPECT_TRUE(pFirst->command("channel[0].mute", false).value("ok").toBool());
    // Each session keeps its own identifiers: the same command_id on another
    // session is a new command there.
    TestClient sixth(port());
    sixth.hello();
    EXPECT_TRUE(pFirst->command("channel[0].mute", true, "shared-id").value("ok").toBool());
    EXPECT_TRUE(sixth.command("mixxx.deck[0].pfl", false, "shared-id").value("ok").toBool());
}

TEST_F(MmccAdapterTest, ARejectedConfigurationOpensNothing) {
    startMixxx();
    for (int field = 0; field < 5; ++field) {
        MmccAdapterConfig config = testConfig();
        switch (field) {
        case 0:
            config.meterRateHz = 0;
            break;
        case 1:
            config.meterRateHz = 61;
            break;
        case 2:
            config.maxSessions = 0;
            break;
        case 3:
            config.maxCommandsPerSession = 0;
            break;
        default:
            config.maxOutboundQueueSize = 0;
            break;
        }
        EXPECT_FALSE(config.isValid());
        MmccAdapter adapter(config, &PlayerInfo::instance());
        EXPECT_FALSE(adapter.isListening());
        EXPECT_EQ(0, adapter.serverPort());
    }
    EXPECT_TRUE(testConfig().isValid());
}

// --- Lifetime ---

TEST_F(MmccAdapterTest, DestroyingTheAdapterClosesItsSocketsBeforeTheControlsGo) {
    start();
    TestClient client(port());
    client.hello();
    client.subscribeAll(m_pAdapter->readablePaths());
    const quint16 usedPort = port();
    // A change is on its way when the adapter dies.
    FakeMixxx::set("[Channel1]", "mute", 1.0);
    m_pAdapter.reset();
    EXPECT_TRUE(client.waitDisconnected());
    // Nothing listens any more.
    QTcpSocket probe;
    probe.connectToHost(QHostAddress::LocalHost, usedPort);
    EXPECT_FALSE(probe.waitForConnected(300));
    // The shutdown order of CoreServices::finalize: adapter, then players,
    // engine and effects (the controls), then PlayerInfo.
    FakeMixxx::set("[Channel1]", "mute", 0.0);
    m_pMixxx->clear();
    pumpFor(20);
}

TEST_F(MmccAdapterTest, TheAdapterSurvivesTheControlsAndPlayerInfoGoingFirst) {
    // The wrong order must not crash either: the proxies keep the control
    // data alive and the adapter holds its own track pointers.
    start();
    TestClient client(port());
    client.hello();
    client.subscribeAll(m_pAdapter->readablePaths());
    FakeMixxx::set("[Channel1]", "vu_meter_left", 0.5);
    m_pMixxx->clear();
    PlayerInfo::destroy();
    m_pAdapter->pollContinuous();
    m_pAdapter->flushDiscrete();
    client.command("channel[0].mute", true);
    EXPECT_FALSE(client.hello().isEmpty());
    client.drain();
    m_pAdapter.reset();
    EXPECT_TRUE(client.waitDisconnected());
}

TEST_F(MmccAdapterTest, AnAdapterWithoutMixxxReportsEverythingUnavailable) {
    // No controls at all and no PlayerInfo: every nullable path is null and
    // nothing asserts.
    MmccAdapterConfig config = testConfig();
    MmccAdapter adapter(config, nullptr);
    ASSERT_TRUE(adapter.isListening());
    TestClient client(adapter.serverPort());
    const QJsonObject values = client.hello();
    EXPECT_EQ(139, values.size());
    EXPECT_TRUE(values.value("channel[0].volume").isNull());
    EXPECT_TRUE(values.value("mixxx.sampler[0].play").isNull());
    EXPECT_QSTRING_EQ("stale_object", errorCode(client.command("mixxx.crossfader", 0.5)));
    EXPECT_QSTRING_EQ("stale_object", errorCode(client.command("channel[0].mute", true)));
}

} // namespace
