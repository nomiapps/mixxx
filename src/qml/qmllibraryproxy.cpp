#include "qml/qmllibraryproxy.h"

#include <QAbstractItemModel>

#include "library/basesqltablemodel.h"
#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/sidebarmodel.h"
#include "moc_qmllibraryproxy.cpp"

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
