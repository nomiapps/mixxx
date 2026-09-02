#pragma once
#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <memory>

#include "qml/qmllibrarytracklistmodel.h"
#include "util/parented_ptr.h"

class Library;

namespace mixxx {
namespace qml {

class QmlLibraryTrackListModel;

class QmlLibraryProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(mixxx::qml::QmlLibraryTrackListModel* model MEMBER m_pModelProperty CONSTANT)
    Q_PROPERTY(QAbstractItemModel* sidebarModel READ sidebarModel CONSTANT)
    Q_PROPERTY(QVariantList smartCrates READ smartCrates NOTIFY smartCratesChanged)
    QML_NAMED_ELEMENT(Library)
    QML_SINGLETON

  public:
    explicit QmlLibraryProxy(std::shared_ptr<Library> pLibrary, QObject* parent = nullptr);

    /// Filter the track table with the library's native search (same query
    /// syntax as the legacy search box, e.g. artist:foo bpm:>120).
    Q_INVOKABLE void search(const QString& searchText);

    /// The library features tree (Tracks, Auto DJ, Playlists, Crates, ...)
    QAbstractItemModel* sidebarModel() const;

    /// Activate a sidebar entry: switches the track table to that feature's
    /// model (crate, playlist, history, ...) when it is a track table.
    Q_INVOKABLE void activateSidebarIndex(const QModelIndex& index);

    /// Smart crates: named saved library searches. Each entry is a
    /// {name, query} map. Persisted per-profile as qml_smart_crates.json.
    QVariantList smartCrates() const {
        return m_smartCrates;
    }
    /// Save a smart crate (adds, or replaces one with the same name).
    Q_INVOKABLE void addSmartCrate(const QString& name, const QString& query);
    /// Remove the smart crate at the given index.
    Q_INVOKABLE void removeSmartCrate(int index);
    /// Show a smart crate: switch to the library and apply its saved search.
    Q_INVOKABLE void activateSmartCrate(int index);

    /// Create a new (real) crate with the given name. No-op if a crate with
    /// that name already exists or the name is blank.
    Q_INVOKABLE void createCrate(const QString& name);
    /// Rename the crate currently named oldName to newName.
    Q_INVOKABLE void renameCrate(const QString& oldName, const QString& newName);
    /// Delete the crate with the given name.
    Q_INVOKABLE void deleteCrate(const QString& name);

  signals:
    void smartCratesChanged();

  public:
    static QmlLibraryProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerLibrary(std::shared_ptr<Library> pLibrary) {
        s_pLibrary = std::move(pLibrary);
    }

  private:
    void loadSmartCrates();
    void saveSmartCrates();

    static inline std::shared_ptr<Library> s_pLibrary;
    QVariantList m_smartCrates;

    std::shared_ptr<Library> m_pLibrary;

    /// This needs to be a plain pointer because it's used as a `Q_PROPERTY` member variable.
    QmlLibraryTrackListModel* m_pModelProperty;
};

} // namespace qml
} // namespace mixxx
