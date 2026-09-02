#include "qml/qmllibraryproxy.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "library/basesqltablemodel.h"
#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/sidebarmodel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/crate.h"
#include "library/trackset/crate/cratestorage.h"
#include "moc_qmllibraryproxy.cpp"
#include "util/cmdlineargs.h"

namespace {
QString settingsFilePath(const QString& fileName) {
    return QDir(CmdlineArgs::Instance().getSettingsPath()).filePath(fileName);
}
QString smartCratesPath() {
    return settingsFilePath(QStringLiteral("qml_smart_crates.json"));
}
QString viewStatePath() {
    return settingsFilePath(QStringLiteral("qml_view_state.json"));
}
} // namespace

namespace mixxx {
namespace qml {

QmlLibraryProxy::QmlLibraryProxy(std::shared_ptr<Library> pLibrary, QObject* parent)
        : QObject(parent),
          m_pLibrary(pLibrary),
          m_pModelProperty(new QmlLibraryTrackListModel(m_pLibrary->trackTableModel(), this)) {
    // Follow the active feature: when the sidebar activates a crate,
    // playlist, history etc., swap the QML track list onto its model.
    connect(m_pLibrary.get(),
            &Library::showTrackModel,
            this,
            [this](QAbstractItemModel* pModel) {
                auto* pSqlModel = qobject_cast<BaseSqlTableModel*>(pModel);
                if (pSqlModel && pSqlModel != m_pModelProperty->sourceModel()) {
                    pSqlModel->select();
                    m_pModelProperty->setSourceModel(pSqlModel);
                }
            });
    loadSmartCrates();
}

void QmlLibraryProxy::addSmartCrate(const QString& name, const QString& query) {
    if (name.trimmed().isEmpty()) {
        return;
    }
    // Replace an existing crate with the same name, otherwise append.
    for (int i = 0; i < m_smartCrates.size(); ++i) {
        if (m_smartCrates.at(i).toMap().value(QStringLiteral("name")).toString() ==
                name) {
            m_smartCrates.removeAt(i);
            break;
        }
    }
    QVariantMap entry;
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("query"), query);
    m_smartCrates.append(entry);
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

void QmlLibraryProxy::activateSmartCrate(int index) {
    if (index < 0 || index >= m_smartCrates.size()) {
        return;
    }
    const QString query =
            m_smartCrates.at(index).toMap().value(QStringLiteral("query")).toString();
    m_pLibrary->searchTracksInCollection(query);
}

void QmlLibraryProxy::createCrate(const QString& name) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    TrackCollection* pCollection =
            m_pLibrary->trackCollectionManager()->internalCollection();
    VERIFY_OR_DEBUG_ASSERT(pCollection) {
        return;
    }
    // Don't create a duplicate name.
    if (pCollection->crates().readCrateByName(trimmed)) {
        return;
    }
    Crate crate;
    crate.setName(trimmed);
    pCollection->insertCrate(crate);
}

void QmlLibraryProxy::renameCrate(const QString& oldName, const QString& newName) {
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    TrackCollection* pCollection =
            m_pLibrary->trackCollectionManager()->internalCollection();
    VERIFY_OR_DEBUG_ASSERT(pCollection) {
        return;
    }
    Crate crate;
    if (!pCollection->crates().readCrateByName(oldName, &crate)) {
        return;
    }
    crate.setName(trimmed);
    pCollection->updateCrate(crate);
}

void QmlLibraryProxy::deleteCrate(const QString& name) {
    TrackCollection* pCollection =
            m_pLibrary->trackCollectionManager()->internalCollection();
    VERIFY_OR_DEBUG_ASSERT(pCollection) {
        return;
    }
    Crate crate;
    if (!pCollection->crates().readCrateByName(name, &crate)) {
        return;
    }
    pCollection->deleteCrate(crate.getId());
}

QStringList QmlLibraryProxy::crateNames() const {
    QStringList names;
    TrackCollection* pCollection =
            m_pLibrary->trackCollectionManager()->internalCollection();
    VERIFY_OR_DEBUG_ASSERT(pCollection) {
        return names;
    }
    CrateSelectResult crates = pCollection->crates().selectCrates();
    Crate crate;
    while (crates.populateNext(&crate)) {
        names.append(crate.getName());
    }
    return names;
}

void QmlLibraryProxy::addTrackUrlToCrate(const QUrl& trackUrl, const QString& crateName) {
    TrackCollection* pCollection =
            m_pLibrary->trackCollectionManager()->internalCollection();
    VERIFY_OR_DEBUG_ASSERT(pCollection) {
        return;
    }
    Crate crate;
    if (!pCollection->crates().readCrateByName(crateName, &crate)) {
        return;
    }
    const QList<TrackId> trackIds =
            m_pLibrary->trackCollectionManager()->resolveTrackIdsFromUrls(
                    QList<QUrl>{trackUrl}, /*addMissing*/ true);
    if (trackIds.isEmpty()) {
        return;
    }
    pCollection->addCrateTracks(crate.getId(), trackIds);
}

void QmlLibraryProxy::saveViewState(const QString& key, const QVariant& value) {
    // QML passes plain JS objects as a QJSValue wrapped in the QVariant;
    // unwrap to a QVariantMap so fromVariant produces a JSON object, not null.
    QVariant v = value;
    if (v.canConvert<QJSValue>()) {
        v = v.value<QJSValue>().toVariant();
    }
    QJsonObject root;
    QFile in(viewStatePath());
    if (in.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(in.readAll()).object();
        in.close();
    }
    root.insert(key, QJsonValue::fromVariant(v));
    QFile out(viewStatePath());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    out.close();
}

QVariant QmlLibraryProxy::loadViewState(const QString& key) const {
    QFile file(viewStatePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    return root.value(key).toVariant();
}

void QmlLibraryProxy::loadSmartCrates() {
    QFile file(smartCratesPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    m_smartCrates.clear();
    const QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), o.value(QStringLiteral("name")).toString());
        entry.insert(QStringLiteral("query"), o.value(QStringLiteral("query")).toString());
        m_smartCrates.append(entry);
    }
    emit smartCratesChanged();
}

void QmlLibraryProxy::saveSmartCrates() {
    QJsonArray arr;
    for (const QVariant& v : std::as_const(m_smartCrates)) {
        const QVariantMap m = v.toMap();
        QJsonObject o;
        o.insert(QStringLiteral("name"), m.value(QStringLiteral("name")).toString());
        o.insert(QStringLiteral("query"), m.value(QStringLiteral("query")).toString());
        arr.append(o);
    }
    QFile file(smartCratesPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "QmlLibraryProxy: cannot save smart crates to"
                   << file.fileName();
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
}

QAbstractItemModel* QmlLibraryProxy::sidebarModel() const {
    return m_pLibrary->sidebarModel();
}

void QmlLibraryProxy::activateSidebarIndex(const QModelIndex& index) {
    SidebarModel* pSidebar = m_pLibrary->sidebarModel();
    VERIFY_OR_DEBUG_ASSERT(pSidebar) {
        return;
    }
    pSidebar->pressed(index);
    pSidebar->clicked(index);
}

void QmlLibraryProxy::search(const QString& searchText) {
    m_pLibrary->trackTableModel()->search(searchText);
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
    return new QmlLibraryProxy(s_pLibrary, pQmlEngine);
}

} // namespace qml
} // namespace mixxx
