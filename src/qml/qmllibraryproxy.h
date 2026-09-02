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

    static QmlLibraryProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerLibrary(std::shared_ptr<Library> pLibrary) {
        s_pLibrary = std::move(pLibrary);
    }

  private:
    static inline std::shared_ptr<Library> s_pLibrary;

    std::shared_ptr<Library> m_pLibrary;

    /// This needs to be a plain pointer because it's used as a `Q_PROPERTY` member variable.
    QmlLibraryTrackListModel* m_pModelProperty;
};

} // namespace qml
} // namespace mixxx
