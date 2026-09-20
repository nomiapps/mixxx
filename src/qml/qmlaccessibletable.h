#pragma once

#include <private/qaccessiblequickitem_p.h>

#include <QAccessible>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <memory>

class QAbstractItemModel;
class QItemSelectionModel;
class QQuickTableView;

namespace mixxx {
namespace qml {

/// Set this property to true on a QML TableView to give it a real accessible table.
inline constexpr const char* kAccessibleTableProperty = "mixxxAccessibleTable";

class AccessibleTable;

/// Model and selection signals for AccessibleTable, which is not a QObject.
class AccessibleTableWatcher : public QObject {
    Q_OBJECT
  public:
    AccessibleTableWatcher(AccessibleTable* pTable, QQuickTableView* pView);

    void wireModel();
    void wireSelection();

  public slots:
    void slotModelChanged();
    void slotSelectionModelChanged();
    void slotStructureChanged();
    void slotCurrentChanged(const QModelIndex& current, const QModelIndex& previous);

  private:
    AccessibleTable* m_pTable;
    QQuickTableView* m_pView;
    QPointer<QAbstractItemModel> m_pModel;
    QPointer<QItemSelectionModel> m_pSelection;
};

/// A QML TableView as a screen reader expects a table: rows, columns, column headers,
/// cells with their text and selection, and focus following the current row.
///
/// Qt's own bridge for Quick items has no table interface, so a TableView reaches an
/// assistive technology as one child per instantiated delegate: no rows, no headers,
/// and nothing for the rows scrolled out of view. This reads everything from the
/// model instead, so every row is reachable, and uses the delegates only for geometry.
///
/// Columns: when the model has a `columns` list whose entries carry `hidden` (the
/// library's QmlLibraryTrackListModel does), hidden columns are left out; table column
/// i is then the i-th visible model column.
class AccessibleTable : public QAccessibleQuickItem, public QAccessibleTableInterface {
  public:
    explicit AccessibleTable(QQuickTableView* pView);
    ~AccessibleTable() override;

    // QAccessibleInterface
    QAccessible::Role role() const override;
    QString text(QAccessible::Text t) const override;
    int childCount() const override;
    QAccessibleInterface* child(int index) const override;
    int indexOfChild(const QAccessibleInterface* pChild) const override;
    QAccessibleInterface* childAt(int x, int y) const override;

    // QAccessibleTableInterface
    QAccessibleInterface* caption() const override;
    QAccessibleInterface* summary() const override;
    QAccessibleInterface* cellAt(int row, int column) const override;
    int selectedCellCount() const override;
    QList<QAccessibleInterface*> selectedCells() const override;
    QString columnDescription(int column) const override;
    QString rowDescription(int row) const override;
    int selectedColumnCount() const override;
    int selectedRowCount() const override;
    int columnCount() const override;
    int rowCount() const override;
    QList<int> selectedColumns() const override;
    QList<int> selectedRows() const override;
    bool isColumnSelected(int column) const override;
    bool isRowSelected(int row) const override;
    bool selectRow(int row) override;
    bool selectColumn(int column) override;
    bool unselectRow(int row) override;
    bool unselectColumn(int column) override;
    void modelChange(QAccessibleTableModelChangeEvent* pEvent) override;

    // For the cells and the watcher.
    QQuickTableView* view() const;
    QAbstractItemModel* model() const;
    QItemSelectionModel* selectionModel() const;
    /// Model column of table column `column`, or -1.
    int modelColumn(int column) const;
    /// Table column of model column `column`, or -1 when it is hidden.
    int tableColumn(int modelColumn) const;
    QAccessibleInterface* headerAt(int column) const;
    void clearCells();

  protected:
    void* interface_cast(QAccessible::InterfaceType t) override;

  private:
    QList<int> visibleColumns() const;

    // The caches first: members are built in this order, and the watcher reads the
    // model while it is built.
    mutable QHash<quint64, QAccessible::Id> m_cells;
    mutable QHash<int, QAccessible::Id> m_headers;
    std::unique_ptr<AccessibleTableWatcher> m_watcher;
};

/// For QAccessible::installFactory.
QAccessibleInterface* accessibleTableFactory(const QString& className, QObject* pObject);

} // namespace qml
} // namespace mixxx
