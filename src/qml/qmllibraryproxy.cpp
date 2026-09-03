#include "qml/qmllibraryproxy.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QStringList>
#include <cmath>

#include "control/controlobject.h"
#include "library/library.h"
#include "library/library_prefs.h"
#include "library/librarytablemodel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/crate.h"
#include "library/trackset/crate/cratestorage.h"
#ifdef __ENGINEPRIME__
#include "library/export/libraryexporter.h"
#endif
#include "moc_qmllibraryproxy.cpp"
#include "preferences/colorpalettesettings.h"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "qmltrackproxy.h"
#include "track/cue.h"
#include "track/track.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

namespace {
const ConfigKey kHotcueDefaultColorIndexConfigKey("[Controls]", "HotcueDefaultColorIndex");
const ConfigKey kLoopDefaultColorIndexConfigKey("[Controls]", "LoopDefaultColorIndex");
const ConfigKey kJumpDefaultColorIndexConfigKey("[Controls]", "jump_default_color_index");

constexpr mixxx::audio::FrameDiff_t kMinimumAudibleLoopSizeFrames = 150;

const QString kSmartCratesFileName = QStringLiteral("qml_smart_crates.json");
const QString kSmartCrateNameKey = QStringLiteral("name");
const QString kSmartCrateQueryKey = QStringLiteral("query");

QString smartCratesFilePath() {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pConfig) {
        return {};
    }
    return QDir(pConfig->getSettingsPath()).filePath(kSmartCratesFileName);
}

CuePointer findDeckHotcue(QmlTrackProxy* track, int hotcueNumber) {
    if (!track || !track->internal() || hotcueNumber <= 0) {
        return {};
    }
    return track->internal()->findHotcueByIndex(hotcueNumber - 1);
}

int defaultColorIndexForType(UserSettingsPointer pConfig, mixxx::CueType cueType) {
    switch (cueType) {
    case mixxx::CueType::Loop:
        return pConfig->getValue(kLoopDefaultColorIndexConfigKey, -1);
    case mixxx::CueType::Jump:
        return pConfig->getValue(kJumpDefaultColorIndexConfigKey, -1);
    default:
        return pConfig->getValue(kHotcueDefaultColorIndexConfigKey, -1);
    }
}

mixxx::RgbColor defaultColorForType(
        UserSettingsPointer pConfig,
        const ColorPalette& palette,
        mixxx::CueType cueType) {
    const int colorIndex = defaultColorIndexForType(pConfig, cueType);
    return (colorIndex < 0 || colorIndex >= palette.size())
            ? palette.defaultColor()
            : palette.at(colorIndex);
}

void updateCueTypeAndColorIfDefault(
        UserSettingsPointer pConfig,
        const CuePointer& pCue,
        mixxx::CueType newType) {
    VERIFY_OR_DEBUG_ASSERT(pConfig && pCue) {
        return;
    }

    ColorPaletteSettings colorPaletteSettings(pConfig);
    const ColorPalette palette = colorPaletteSettings.getHotcueColorPalette();
    const mixxx::RgbColor oldDefaultColor =
            defaultColorForType(pConfig, palette, pCue->getType());
    const bool cueUsesOldDefaultColor = pCue->getColor() == oldDefaultColor;

    pCue->setType(newType);
    if (cueUsesOldDefaultColor) {
        pCue->setColor(defaultColorForType(pConfig, palette, newType));
    }
}

mixxx::audio::FramePos getCurrentPlayPositionWithQuantize(
        const TrackPointer& pTrack,
        const QString& group) {
    VERIFY_OR_DEBUG_ASSERT(pTrack) {
        return mixxx::audio::kInvalidFramePos;
    }

    const double trackSamples = ControlObject::get(
            ConfigKey(group, QStringLiteral("track_samples")));
    auto position = mixxx::audio::FramePos::fromEngineSamplePos(
            ControlObject::get(ConfigKey(group, QStringLiteral("playposition"))) *
            trackSamples);
    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    if (ControlObject::get(ConfigKey(group, QStringLiteral("quantize"))) > 0 && pBeats) {
        mixxx::audio::FramePos nextBeatPosition;
        mixxx::audio::FramePos prevBeatPosition;
        pBeats->findPrevNextBeats(position, &prevBeatPosition, &nextBeatPosition, false);
        return (nextBeatPosition - position > position - prevBeatPosition)
                ? prevBeatPosition
                : nextBeatPosition;
    }
    return position;
}
} // namespace

QmlLibraryScannerProxy::QmlLibraryScannerProxy(LibraryScanner* libraryScanner,
        TrackCollectionManager* trackCollectionManager,
        QObject* parent)
        : QObject(parent),
          m_pLibraryScanner(libraryScanner),
          m_running(trackCollectionManager->isLibraryScanActive()),
          m_cancelling(false) {
    connect(libraryScanner,
            &LibraryScanner::progressLoading,
            this,
            &QmlLibraryScannerProxy::progress);
    connect(libraryScanner,
            &LibraryScanner::scanStarted,
            this,
            &QmlLibraryScannerProxy::started);
    connect(libraryScanner,
            &LibraryScanner::scanFinished,
            this,
            &QmlLibraryScannerProxy::finished);
    connect(this,
            &QmlLibraryScannerProxy::requestCancel,
            libraryScanner,
            &LibraryScanner::slotCancel);

    // Properties
    connect(libraryScanner,
            &LibraryScanner::scanStarted,
            this,
            [this]() {
                m_cancelling = false;
                m_running = true;
                emit stateChanged();
            });
    connect(libraryScanner,
            &LibraryScanner::scanFinished,
            this,
            [this]() {
                m_cancelling = false;
                m_running = false;
                emit stateChanged();
            });
}

QmlLibraryProxy::QmlLibraryProxy(QObject* parent)
        : QObject(parent),
          m_pModelProperty(new QmlLibraryTrackListModel(
                  QList<QmlLibraryTrackListColumn*>{}, s_pLibrary->trackTableModel(), this)),
          m_pScanner(new QmlLibraryScannerProxy(
                  s_pLibrary->trackCollectionManager()->scanner(),
                  s_pLibrary->trackCollectionManager(),
                  this)),
          m_pTrackCollectionManager(s_pLibrary->trackCollectionManager()) {
    connect(m_pScanner,
            &QmlLibraryScannerProxy::stateChanged,
            this,
            &QmlLibraryProxy::libraryScanActiveChanged);
    TrackCollectionManager* pTrackCollectionManager =
            s_pLibrary->trackCollectionManager();
    VERIFY_OR_DEBUG_ASSERT(pTrackCollectionManager) {
        return;
    }
    connect(pTrackCollectionManager,
            &TrackCollectionManager::libraryScanSummary,
            this,
            [this](const LibraryScanResultSummary&) {
                deliverPendingLibraryScanSummary();
            });
    deliverPendingLibraryScanSummary();
    loadSmartCrates();
#ifdef __ENGINEPRIME__
    m_pLibraryExporter = s_pLibrary->makeLibraryExporter(nullptr);
    connect(s_pLibrary.get(),
            &Library::exportLibrary,
            m_pLibraryExporter.get(),
            &mixxx::LibraryExporter::slotRequestExport);
    connect(s_pLibrary.get(),
            &Library::exportCrate,
            m_pLibraryExporter.get(),
            &mixxx::LibraryExporter::slotRequestExportWithInitialCrate);
    connect(s_pLibrary.get(),
            &Library::exportPlaylist,
            m_pLibraryExporter.get(),
            &mixxx::LibraryExporter::slotRequestExportWithInitialPlaylist);
#endif
}

QmlLibraryProxy::~QmlLibraryProxy() = default;

void QmlLibraryProxy::deliverPendingLibraryScanSummary() {
    const auto pendingResult = m_pTrackCollectionManager->takePendingLibraryScanSummary();
    if (!pendingResult) {
        return;
    }
    const auto& result = *pendingResult;
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig ||
            !pConfig->getValue<bool>(
                    mixxx::library::prefs::kShowScanSummaryConfigKey, true)) {
        return;
    }
    if (result.autoscan &&
            result.numNewTracks == 0 &&
            result.numNewMissingTracks == 0 &&
            result.numRediscoveredTracks == 0) {
        return;
    }

    const QString title = tr("Library scan finished");
    if (result.noDirectoriesConfigured) {
        emit libraryScanSummaryAvailable(title,
                tr("No music directories configured for scanning."),
                tr("Add directories in the library preferences."));
        return;
    }

    const QString text = tr("Scan took %1").arg(result.durationString);
    QStringList details;
    if (result.numNewTracks == 0 &&
            result.numMovedTracks == 0 &&
            result.numNewMissingTracks == 0 &&
            result.numRediscoveredTracks == 0) {
        details.append(tr("No changes detected."));
    } else {
        if (result.numNewTracks != 0) {
            details.append(tr("%n new track(s) found", nullptr, result.numNewTracks));
        }
        if (result.numMovedTracks != 0) {
            details.append(
                    tr("%n moved track(s) detected", nullptr, result.numMovedTracks));
        }
        if (result.numNewMissingTracks != 0) {
            details.append(tr("%n track(s) missing (%1 total)",
                    nullptr,
                    result.numNewMissingTracks)
                            .arg(result.numMissingTracks));
        }
        if (result.numRediscoveredTracks != 0) {
            details.append(tr("%n track(s) rediscovered",
                    nullptr,
                    result.numRediscoveredTracks));
        }
    }
    details.append(tr("%n track(s) in total", nullptr, result.tracksTotal));
    emit libraryScanSummaryAvailable(title, text, details.join(QLatin1Char('\n')));
}

QmlLibraryTrackListModel* QmlLibraryProxy::model() const {
    return make_qml_owned<QmlLibraryTrackListModel>(
            QList<QmlLibraryTrackListColumn*>{}, s_pLibrary->trackTableModel())
            .get();
}
void QmlLibraryProxy::analyze(const QmlTrackProxy* track) const {
    VERIFY_OR_DEBUG_ASSERT(track && track->internal()) {
        return;
    }
    emit s_pLibrary->analyzeTracks({track->internal()->getId()});
}

void QmlLibraryProxy::createCrate() {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return;
    }
    s_pLibrary->slotCreateCrate();
}

void QmlLibraryProxy::createPlaylist() {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return;
    }
    s_pLibrary->slotCreatePlaylist();
}

bool QmlLibraryProxy::enginePrimeExportAvailable() const {
#ifdef __ENGINEPRIME__
    return true;
#else
    return false;
#endif
}

void QmlLibraryProxy::exportLibrary() {
#ifdef __ENGINEPRIME__
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return;
    }
    emit s_pLibrary->exportLibrary();
#endif
}

void QmlLibraryProxy::rescanLibrary() {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return;
    }
    if (libraryScanActive()) {
        return;
    }
    TrackCollectionManager* pTrackCollectionManager =
            s_pLibrary->trackCollectionManager();
    VERIFY_OR_DEBUG_ASSERT(pTrackCollectionManager) {
        return;
    }
    pTrackCollectionManager->startLibraryScan();
}

void QmlLibraryProxy::searchInCurrentView() {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return;
    }
    s_pLibrary->slotSearchInCurrentView();
}

void QmlLibraryProxy::searchInTracksLibrary() {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return;
    }
    s_pLibrary->slotSearchInAllTracks();
}

void QmlLibraryProxy::showAutoDJ() {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return;
    }
    s_pLibrary->showAutoDJ();
}

QVariantList QmlLibraryProxy::crates() const {
    QVariantList crates;
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return crates;
    }
    TrackCollection* pCollection =
            s_pLibrary->trackCollectionManager()->internalCollection();
    VERIFY_OR_DEBUG_ASSERT(pCollection) {
        return crates;
    }
    CrateSelectResult result = pCollection->crates().selectCrates();
    Crate crate;
    while (result.populateNext(&crate)) {
        crates.append(QVariantMap{
                {QStringLiteral("id"), crate.getId().toVariant()},
                {QStringLiteral("name"), crate.getName()},
                {QStringLiteral("locked"), crate.isLocked()},
        });
    }
    return crates;
}

bool QmlLibraryProxy::addTrackToCrate(const QmlTrackProxy* track, int crateId) {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary && track && track->internal()) {
        return false;
    }
    const CrateId id{QVariant(crateId)};
    if (!id.isValid()) {
        return false;
    }
    TrackCollection* pCollection =
            s_pLibrary->trackCollectionManager()->internalCollection();
    VERIFY_OR_DEBUG_ASSERT(pCollection) {
        return false;
    }
    Crate crate;
    if (!pCollection->crates().readCrateById(id, &crate) || crate.isLocked()) {
        return false;
    }
    return pCollection->addCrateTracks(id, {track->internal()->getId()});
}

void QmlLibraryProxy::addSmartCrate(const QString& name, const QString& query) {
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return;
    }
    // Replace an existing entry with the same name, otherwise append.
    for (int i = 0; i < m_smartCrates.size(); ++i) {
        if (m_smartCrates.at(i).toMap().value(kSmartCrateNameKey).toString() ==
                trimmedName) {
            m_smartCrates.removeAt(i);
            break;
        }
    }
    m_smartCrates.append(QVariantMap{
            {kSmartCrateNameKey, trimmedName},
            {kSmartCrateQueryKey, query},
    });
    saveSmartCrates();
    emit smartCratesChanged();
}

void QmlLibraryProxy::removeSmartCrate(int index) {
    if (index < 0 || index >= m_smartCrates.size()) {
        return;
    }
    m_smartCrates.removeAt(index);
    saveSmartCrates();
    emit smartCratesChanged();
}

void QmlLibraryProxy::loadSmartCrates() {
    const QString filePath = smartCratesFilePath();
    if (filePath.isEmpty()) {
        return;
    }
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonArray entries = QJsonDocument::fromJson(file.readAll()).array();
    m_smartCrates.clear();
    for (const auto& entry : entries) {
        const QJsonObject object = entry.toObject();
        const QString name = object.value(kSmartCrateNameKey).toString();
        if (name.isEmpty()) {
            continue;
        }
        m_smartCrates.append(QVariantMap{
                {kSmartCrateNameKey, name},
                {kSmartCrateQueryKey, object.value(kSmartCrateQueryKey).toString()},
        });
    }
}

void QmlLibraryProxy::saveSmartCrates() const {
    const QString filePath = smartCratesFilePath();
    if (filePath.isEmpty()) {
        return;
    }
    QJsonArray entries;
    for (const QVariant& entry : std::as_const(m_smartCrates)) {
        const QVariantMap map = entry.toMap();
        entries.append(QJsonObject{
                {kSmartCrateNameKey, map.value(kSmartCrateNameKey).toString()},
                {kSmartCrateQueryKey, map.value(kSmartCrateQueryKey).toString()},
        });
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to save smart crates to" << filePath;
        return;
    }
    file.write(QJsonDocument(entries).toJson(QJsonDocument::Indented));
}

QString QmlLibraryProxy::deckHotcueLabel(
        QmlTrackProxy* track,
        int hotcueNumber) const {
    const CuePointer pCue = findDeckHotcue(track, hotcueNumber);
    return pCue ? pCue->getLabel() : QString();
}

bool QmlLibraryProxy::setDeckHotcueLabel(
        QmlTrackProxy* track,
        int hotcueNumber,
        const QString& label) {
    const CuePointer pCue = findDeckHotcue(track, hotcueNumber);
    if (!pCue) {
        return false;
    }
    pCue->setLabel(label);
    return true;
}

bool QmlLibraryProxy::setDeckHotcueType(
        QmlTrackProxy* track,
        const QString& group,
        int hotcueNumber,
        const QString& action) {
    const CuePointer pCue = findDeckHotcue(track, hotcueNumber);
    if (!track || !track->internal() || !pCue) {
        return false;
    }

    UserSettingsPointer pConfig = QmlConfigProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pConfig) {
        return false;
    }

    const TrackPointer pTrack = track->internal();
    if (action == QStringLiteral("standard")) {
        if (pCue->getType() != mixxx::CueType::HotCue) {
            updateCueTypeAndColorIfDefault(pConfig, pCue, mixxx::CueType::HotCue);
        }
        return true;
    }

    if (action == QStringLiteral("loop-auto")) {
        Cue::StartAndEndPositions cueStartEnd = pCue->getStartAndEndPosition();
        if (pCue->getType() == mixxx::CueType::Jump) {
            const auto endPosition = cueStartEnd.endPosition;
            if (cueStartEnd.endPosition < cueStartEnd.startPosition) {
                cueStartEnd.endPosition = cueStartEnd.startPosition;
                cueStartEnd.startPosition = endPosition;
            }
            pCue->setStartAndEndPosition(cueStartEnd.startPosition, cueStartEnd.endPosition);
        }
        if (!cueStartEnd.endPosition.isValid() ||
                cueStartEnd.endPosition <= cueStartEnd.startPosition) {
            const double beatloopSize = ControlObject::get(
                    ConfigKey(group, QStringLiteral("beatloop_size")));
            const mixxx::BeatsPointer pBeats = pTrack->getBeats();
            if (beatloopSize <= 0 || !pBeats) {
                return false;
            }
            const auto position = pBeats->findNBeatsFromPosition(
                    cueStartEnd.startPosition, beatloopSize);
            if (position <= pCue->getPosition()) {
                return false;
            }
            pCue->setEndPosition(position);
        }
        updateCueTypeAndColorIfDefault(pConfig, pCue, mixxx::CueType::Loop);
        return true;
    }

    if (action == QStringLiteral("loop-manual")) {
        if (pCue->getType() == mixxx::CueType::Jump &&
                pCue->getPosition() > pCue->getEndPosition()) {
            Cue::StartAndEndPositions cueStartEnd = pCue->getStartAndEndPosition();
            const auto endPosition = cueStartEnd.endPosition;
            cueStartEnd.endPosition = cueStartEnd.startPosition;
            cueStartEnd.startPosition = endPosition;
            pCue->setStartAndEndPosition(cueStartEnd.startPosition, cueStartEnd.endPosition);
        }
        const auto newPosition = getCurrentPlayPositionWithQuantize(pTrack, group);
        if (newPosition <= pCue->getPosition()) {
            return false;
        }
        pCue->setEndPosition(newPosition);
        updateCueTypeAndColorIfDefault(pConfig, pCue, mixxx::CueType::Loop);
        return true;
    }

    if (action == QStringLiteral("jump-auto")) {
        Cue::StartAndEndPositions cueStartEnd = pCue->getStartAndEndPosition();
        if (pCue->getType() == mixxx::CueType::Loop ||
                pCue->getType() == mixxx::CueType::Jump) {
            const auto endPosition = cueStartEnd.endPosition;
            cueStartEnd.endPosition = cueStartEnd.startPosition;
            cueStartEnd.startPosition = endPosition;
        }
        if (!cueStartEnd.endPosition.isValid()) {
            const auto newPosition = getCurrentPlayPositionWithQuantize(pTrack, group);
            if (std::abs(newPosition - cueStartEnd.startPosition) <=
                    kMinimumAudibleLoopSizeFrames) {
                return false;
            }
            cueStartEnd.endPosition = newPosition;
        }
        pCue->setStartAndEndPosition(cueStartEnd.startPosition, cueStartEnd.endPosition);
        updateCueTypeAndColorIfDefault(pConfig, pCue, mixxx::CueType::Jump);
        return true;
    }

    if (action == QStringLiteral("jump-manual")) {
        Cue::StartAndEndPositions cueStartEnd = pCue->getStartAndEndPosition();
        const auto newPosition = getCurrentPlayPositionWithQuantize(pTrack, group);
        if (newPosition == cueStartEnd.startPosition) {
            return false;
        }
        cueStartEnd.endPosition = newPosition;
        pCue->setStartAndEndPosition(cueStartEnd.startPosition, cueStartEnd.endPosition);
        updateCueTypeAndColorIfDefault(pConfig, pCue, mixxx::CueType::Jump);
        return true;
    }

    return false;
}

void QmlLibraryProxy::cleanupDeckHotcuePopup(
        QmlTrackProxy* track,
        int hotcueNumber) {
    const CuePointer pCue = findDeckHotcue(track, hotcueNumber);
    if (pCue &&
            pCue->getType() == mixxx::CueType::HotCue &&
            pCue->getEndPosition().isValid()) {
        pCue->setEndPosition(mixxx::audio::FramePos());
    }
}

// static
QmlLibraryProxy* QmlLibraryProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    // The implementation of this method is mostly taken from the code example
    // that shows the replacement for `qmlRegisterSingletonInstance()` when
    // using `QML_SINGLETON`.
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON

    // The instance has to exist before it is used. We cannot replace it.
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        qWarning() << "Library hasn't been registered yet";
        return nullptr;
    }
    return new QmlLibraryProxy(pQmlEngine);
}

QmlLibraryProxy::AddResult QmlLibraryProxy::addSource(
        const QUrl& newPath) {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        qWarning() << "Library hasn't been registered yet!";
        return QmlLibraryProxy::AddResult::InvalidOrMissingDirectory;
    }
    QDir directory(newPath.toLocalFile());
    Sandbox::createSecurityTokenForDir(directory);
    return static_cast<QmlLibraryProxy::AddResult>(
            s_pLibrary->trackCollectionManager()->addDirectory(
                    mixxx::FileInfo(newPath.toLocalFile())));
}

QmlLibraryProxy::RemoveResult QmlLibraryProxy::removeSource(
        const QUrl& oldPath, SourceRemovalType removalType) {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        qWarning() << "Library hasn't been registered yet!";
        return QmlLibraryProxy::RemoveResult::NotFound;
    }

    DirectoryDAO::RemoveResult result =
            s_pLibrary->trackCollectionManager()->removeDirectory(
                    mixxx::FileInfo(oldPath.toLocalFile()));
    if (result != DirectoryDAO::RemoveResult::Ok) {
        return static_cast<QmlLibraryProxy::RemoveResult>(result);
    }

    switch (removalType) {
    case SourceRemovalType::KeepTracks:
        break;
    case SourceRemovalType::HideTracks:
        // Mark all tracks in this directory as deleted but DON'T purge them
        // in case the user re-adds them manually.
        s_pLibrary->trackCollectionManager()->hideAllTracks(oldPath.toLocalFile());
        break;
    case SourceRemovalType::PurgeTracks:
        // The user requested that we purge all metadata.
        s_pLibrary->trackCollectionManager()->purgeAllTracks(oldPath.toLocalFile());
        break;
    default:
        DEBUG_ASSERT(!"unreachable");
    }
    return static_cast<QmlLibraryProxy::RemoveResult>(result);
}

QmlLibraryProxy::RelocateResult QmlLibraryProxy::relinkSource(
        const QUrl& oldPath, const QUrl& newPath) {
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        qWarning() << "Library hasn't been registered yet!";
        return QmlLibraryProxy::RelocateResult::SqlError;
    }
    return static_cast<QmlLibraryProxy::RelocateResult>(
            s_pLibrary->trackCollectionManager()->relocateDirectory(
                    oldPath.toLocalFile(), newPath.toLocalFile()));
}

// Static
qsizetype QmlLibraryProxy::sources_count(QQmlListProperty<QmlLibrarySource>* pList) {
    VERIFY_OR_DEBUG_ASSERT(pList && pList->object && s_pLibrary) {
        return 0;
    }
    return s_pLibrary->trackCollectionManager()
            ->internalCollection()
            ->getRootDirectories()
            .size();
}

// Static
QmlLibrarySource* QmlLibraryProxy::sources_at(
        QQmlListProperty<QmlLibrarySource>* pList, qsizetype index) {
    VERIFY_OR_DEBUG_ASSERT(pList && pList->object) {
        return nullptr;
    }
    VERIFY_OR_DEBUG_ASSERT(s_pLibrary) {
        return nullptr;
    }
    return make_qml_owned<QmlLibrarySource>(
            s_pLibrary->trackCollectionManager()
                    ->internalCollection()
                    ->getRootDirectories()
                    .at(index));
}

// Static
void QmlLibraryProxy::sources_clear(QQmlListProperty<QmlLibrarySource>*) {
    DEBUG_ASSERT(!"unsupported operation");
}

} // namespace qml
} // namespace mixxx
