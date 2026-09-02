#pragma once
#include <QIdentityProxyModel>
#include <QQmlEngine>

class LibraryTableModel;

namespace mixxx {
namespace qml {

class QmlLibraryTrackListModel : public QIdentityProxyModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryTrackListModel)
    QML_UNCREATABLE("Only accessible via Mixxx.Library.model")

  public:
    enum Roles {
        TitleRole = Qt::UserRole,
        ArtistRole,
        AlbumRole,
        AlbumArtistRole,
        FileUrlRole,
        BpmRole,
        KeyRole,
        DurationRole,
        GenreRole,
        YearRole,
    };
    Q_ENUM(Roles);

    QmlLibraryTrackListModel(LibraryTableModel* pModel, QObject* pParent = nullptr);
    ~QmlLibraryTrackListModel() = default;

    QVariant data(const QModelIndex& index, int role) const override;
    int columnCount(const QModelIndex& index = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariant get(int row) const;

    /// Sort the underlying table by a role name ("bpm", "title", ...).
    /// descending=false is ascending. Reuses BaseSqlTableModel::sort.
    Q_INVOKABLE void sortByRole(const QString& roleName, bool descending);
};

} // namespace qml
} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::qml::QmlLibraryTrackListModel*)
