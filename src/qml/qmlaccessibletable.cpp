#include "qml/qmlaccessibletable.h"

#include <private/qquicktableview_p.h>

#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QQmlListReference>
#include <QQuickItem>
#include <QQuickWindow>
#include <algorithm>

#include "moc_qmlaccessibletable.cpp"

namespace mixxx {
namespace qml {

namespace {

quint64 cellKey(int row, int column) {
    return (static_cast<quint64>(static_cast<quint32>(row)) << 32) | static_cast<quint32>(column);
}

/// One cell, or one column header, of an AccessibleTable. Owned by the accessibility
/// cache, created on demand, and deleted by the table when the model changes shape.
class AccessibleTableCell : public QAccessibleInterface, public QAccessibleTableCellInterface {
  public:
    AccessibleTableCell(AccessibleTable* pTable, int row, int column, bool header)
            : m_pTable(pTable),
              m_row(row),
              m_column(column),
              m_header(header) {
    }

    bool isValid() const override {
        return m_pTable && m_pTable->isValid() && m_column < m_pTable->columnCount() &&
                (m_header || m_row < m_pTable->rowCount());
    }
    QObject* object() const override {
        return nullptr;
    }
    QWindow* window() const override {
        return m_pTable->window();
    }
    QAccessibleInterface* childAt(int, int) const override {
        return nullptr;
    }
    QAccessibleInterface* parent() const override {
        return m_pTable;
    }
    QAccessibleInterface* child(int) const override {
        return nullptr;
    }
    int childCount() const override {
        return 0;
    }
    int indexOfChild(const QAccessibleInterface*) const override {
        return -1;
    }

    QString text(QAccessible::Text t) const override {
        if (t != QAccessible::Name || !isValid()) {
            return {};
        }
        if (m_header) {
            return m_pTable->columnDescription(m_column);
        }
        QAbstractItemModel* pModel = m_pTable->model();
        return pModel->data(pModel->index(m_row, m_pTable->modelColumn(m_column)), Qt::DisplayRole)
                .toString();
    }
    void setText(QAccessible::Text, const QString&) override {
    }

    QRect rect() const override {
        if (m_header || !isValid()) {
            return {};
        }
        QQuickItem* pCell = m_pTable->view()->itemAtCell(QPoint(m_pTable->modelColumn(m_column), m_row));
        if (!pCell || !pCell->window()) {
            return {};
        }
        const QRectF scene = pCell->mapRectToScene(QRectF(0, 0, pCell->width(), pCell->height()));
        const QPoint topLeft = pCell->window()->mapToGlobal(scene.topLeft().toPoint());
        return QRect(topLeft, scene.size().toSize());
    }

    QAccessible::Role role() const override {
        return m_header ? QAccessible::ColumnHeader : QAccessible::Cell;
    }

    QAccessible::State state() const override {
        QAccessible::State st;
        if (m_header || !isValid()) {
            return st;
        }
        st.selectable = true;
        st.focusable = true;
        st.selected = isSelected();
        QQuickTableView* pView = m_pTable->view();
        const QItemSelectionModel* pSelection = m_pTable->selectionModel();
        st.focused = pView->hasActiveFocus() && pSelection &&
                pSelection->currentIndex().row() == m_row;
        // A row scrolled out of view has no delegate: still there, just not on screen.
        st.offscreen = pView->itemAtCell(QPoint(m_pTable->modelColumn(m_column), m_row)) == nullptr;
        return st;
    }

    void* interface_cast(QAccessible::InterfaceType t) override {
        if (t == QAccessible::TableCellInterface && !m_header) {
            return static_cast<QAccessibleTableCellInterface*>(this);
        }
        return nullptr;
    }

    // QAccessibleTableCellInterface
    bool isSelected() const override {
        const QItemSelectionModel* pSelection = m_pTable->selectionModel();
        return pSelection && pSelection->isRowSelected(m_row, QModelIndex());
    }
    QList<QAccessibleInterface*> columnHeaderCells() const override {
        QAccessibleInterface* pHeader = m_pTable->headerAt(m_column);
        return pHeader ? QList<QAccessibleInterface*>{pHeader} : QList<QAccessibleInterface*>{};
    }
    QList<QAccessibleInterface*> rowHeaderCells() const override {
        return {};
    }
    int columnIndex() const override {
        return m_column;
    }
    int rowIndex() const override {
        return m_row;
    }
    int columnExtent() const override {
        return 1;
    }
    int rowExtent() const override {
        return 1;
    }
    QAccessibleInterface* table() const override {
        return m_pTable;
    }

    int row() const {
        return m_row;
    }
    int column() const {
        return m_column;
    }
    bool header() const {
        return m_header;
    }

  private:
    AccessibleTable* m_pTable;
    int m_row;
    int m_column;
    bool m_header;
};

} // namespace

AccessibleTableWatcher::AccessibleTableWatcher(AccessibleTable* pTable, QQuickTableView* pView)
        : m_pTable(pTable),
          m_pView(pView) {
    connect(pView, &QQuickTableView::modelChanged, this, &AccessibleTableWatcher::slotModelChanged);
    connect(pView,
            &QQuickTableView::selectionModelChanged,
            this,
            &AccessibleTableWatcher::slotSelectionModelChanged);
    // Wire only: posting a model-change event from here would ask the cache for the
    // view's interface while that interface is still being constructed.
    wireModel();
    wireSelection();
}

void AccessibleTableWatcher::wireModel() {
    if (m_pModel) {
        disconnect(m_pModel, nullptr, this, nullptr);
    }
    m_pModel = m_pTable->model();
    if (!m_pModel) {
        return;
    }
    const auto changed = &AccessibleTableWatcher::slotStructureChanged;
    connect(m_pModel, &QAbstractItemModel::modelReset, this, changed);
    connect(m_pModel, &QAbstractItemModel::layoutChanged, this, changed);
    connect(m_pModel, &QAbstractItemModel::rowsInserted, this, changed);
    connect(m_pModel, &QAbstractItemModel::rowsRemoved, this, changed);
    connect(m_pModel, &QAbstractItemModel::columnsInserted, this, changed);
    connect(m_pModel, &QAbstractItemModel::columnsRemoved, this, changed);
}

void AccessibleTableWatcher::wireSelection() {
    if (m_pSelection) {
        disconnect(m_pSelection, nullptr, this, nullptr);
    }
    m_pSelection = m_pTable->selectionModel();
    if (m_pSelection) {
        connect(m_pSelection,
                &QItemSelectionModel::currentChanged,
                this,
                &AccessibleTableWatcher::slotCurrentChanged);
    }
}

void AccessibleTableWatcher::slotModelChanged() {
    wireModel();
    slotStructureChanged();
}

void AccessibleTableWatcher::slotSelectionModelChanged() {
    wireSelection();
}

void AccessibleTableWatcher::slotStructureChanged() {
    m_pTable->clearCells();
    if (QAccessible::isActive()) {
        QAccessibleTableModelChangeEvent event(m_pView, QAccessibleTableModelChangeEvent::ModelReset);
        QAccessible::updateAccessibility(&event);
    }
}

void AccessibleTableWatcher::slotCurrentChanged(
        const QModelIndex& current, const QModelIndex& previous) {
    Q_UNUSED(previous);
    // Keyboard navigation in the library moves the current row; a screen reader
    // follows focus, so tell it the focus moved to that row's first cell.
    if (!QAccessible::isActive() || !m_pView->hasActiveFocus() || !current.isValid()) {
        return;
    }
    const int column = m_pTable->tableColumn(current.column());
    QAccessibleInterface* pCell = m_pTable->cellAt(current.row(), column < 0 ? 0 : column);
    if (pCell) {
        QAccessibleEvent event(pCell, QAccessible::Focus);
        QAccessible::updateAccessibility(&event);
    }
}

AccessibleTable::AccessibleTable(QQuickTableView* pView)
        : QAccessibleQuickItem(pView),
          m_watcher(std::make_unique<AccessibleTableWatcher>(this, pView)) {
}

AccessibleTable::~AccessibleTable() {
    clearCells();
}

void AccessibleTable::clearCells() {
    for (const QAccessible::Id id : std::as_const(m_cells)) {
        QAccessible::deleteAccessibleInterface(id);
    }
    for (const QAccessible::Id id : std::as_const(m_headers)) {
        QAccessible::deleteAccessibleInterface(id);
    }
    m_cells.clear();
    m_headers.clear();
}

QQuickTableView* AccessibleTable::view() const {
    return static_cast<QQuickTableView*>(item());
}

QAbstractItemModel* AccessibleTable::model() const {
    return qobject_cast<QAbstractItemModel*>(view()->model().value<QObject*>());
}

QItemSelectionModel* AccessibleTable::selectionModel() const {
    return view()->selectionModel();
}

QList<int> AccessibleTable::visibleColumns() const {
    QList<int> columns;
    QAbstractItemModel* pModel = model();
    if (!pModel) {
        return columns;
    }
    const int count = pModel->columnCount();
    QQmlListReference definitions(pModel, "columns");
    for (int c = 0; c < count; ++c) {
        if (definitions.isValid() && c < definitions.count()) {
            const QObject* pDefinition = definitions.at(c);
            if (pDefinition && pDefinition->property("hidden").toBool()) {
                continue;
            }
        }
        columns.append(c);
    }
    return columns;
}

int AccessibleTable::modelColumn(int column) const {
    const QList<int> columns = visibleColumns();
    return column >= 0 && column < columns.size() ? columns[column] : -1;
}

int AccessibleTable::tableColumn(int modelColumn) const {
    return static_cast<int>(visibleColumns().indexOf(modelColumn));
}

void* AccessibleTable::interface_cast(QAccessible::InterfaceType t) {
    if (t == QAccessible::TableInterface) {
        return static_cast<QAccessibleTableInterface*>(this);
    }
    return QAccessibleQuickItem::interface_cast(t);
}

QAccessible::Role AccessibleTable::role() const {
    return QAccessible::Table;
}

QString AccessibleTable::text(QAccessible::Text t) const {
    const QString own = QAccessibleQuickItem::text(t);
    if (t == QAccessible::Name && own.isEmpty()) {
        return QObject::tr("Track list");
    }
    return own;
}

// Children: the column headers first, then the cells row by row -- the order a
// QTableView gives, so tools that walk children rather than the table see the same.
int AccessibleTable::childCount() const {
    const int columns = columnCount();
    return columns + rowCount() * columns;
}

QAccessibleInterface* AccessibleTable::child(int index) const {
    const int columns = columnCount();
    if (index < 0 || columns == 0) {
        return nullptr;
    }
    if (index < columns) {
        return headerAt(index);
    }
    index -= columns;
    return cellAt(index / columns, index % columns);
}

int AccessibleTable::indexOfChild(const QAccessibleInterface* pChild) const {
    const auto* pCell = dynamic_cast<const AccessibleTableCell*>(pChild);
    if (!pCell || pCell->table() != this) {
        return -1;
    }
    const int columns = columnCount();
    return pCell->header() ? pCell->column() : columns + pCell->row() * columns + pCell->column();
}

QAccessibleInterface* AccessibleTable::childAt(int x, int y) const {
    QQuickTableView* pView = view();
    if (!pView->window()) {
        return nullptr;
    }
    const QPointF local = pView->mapFromGlobal(QPointF(x, y));
    const QPoint cell = pView->cellAtPosition(local);
    if (cell.x() < 0 || cell.y() < 0) {
        return nullptr;
    }
    const int column = tableColumn(cell.x());
    return column < 0 ? nullptr : cellAt(cell.y(), column);
}

QAccessibleInterface* AccessibleTable::caption() const {
    return nullptr;
}

QAccessibleInterface* AccessibleTable::summary() const {
    return nullptr;
}

QAccessibleInterface* AccessibleTable::cellAt(int row, int column) const {
    if (row < 0 || column < 0 || row >= rowCount() || column >= columnCount()) {
        return nullptr;
    }
    const quint64 key = cellKey(row, column);
    const auto it = m_cells.constFind(key);
    if (it != m_cells.constEnd()) {
        return QAccessible::accessibleInterface(it.value());
    }
    // The interfaces are handed to the cache as const-correct as Qt allows.
    auto* pCell = new AccessibleTableCell(const_cast<AccessibleTable*>(this), row, column, false);
    const QAccessible::Id id = QAccessible::registerAccessibleInterface(pCell);
    m_cells.insert(key, id);
    return pCell;
}

QAccessibleInterface* AccessibleTable::headerAt(int column) const {
    if (column < 0 || column >= columnCount()) {
        return nullptr;
    }
    const auto it = m_headers.constFind(column);
    if (it != m_headers.constEnd()) {
        return QAccessible::accessibleInterface(it.value());
    }
    auto* pHeader = new AccessibleTableCell(const_cast<AccessibleTable*>(this), -1, column, true);
    m_headers.insert(column, QAccessible::registerAccessibleInterface(pHeader));
    return pHeader;
}

int AccessibleTable::selectedCellCount() const {
    return selectedRowCount() * columnCount();
}

QList<QAccessibleInterface*> AccessibleTable::selectedCells() const {
    QList<QAccessibleInterface*> cells;
    const int columns = columnCount();
    const QList<int> rows = selectedRows();
    for (const int row : rows) {
        for (int c = 0; c < columns; ++c) {
            cells.append(cellAt(row, c));
        }
    }
    return cells;
}

QString AccessibleTable::columnDescription(int column) const {
    QAbstractItemModel* pModel = model();
    const int source = modelColumn(column);
    if (!pModel || source < 0) {
        return {};
    }
    return pModel->headerData(source, Qt::Horizontal, Qt::DisplayRole).toString();
}

QString AccessibleTable::rowDescription(int row) const {
    Q_UNUSED(row);
    return {};
}

int AccessibleTable::selectedColumnCount() const {
    return 0;
}

int AccessibleTable::selectedRowCount() const {
    return static_cast<int>(selectedRows().size());
}

int AccessibleTable::columnCount() const {
    return static_cast<int>(visibleColumns().size());
}

int AccessibleTable::rowCount() const {
    QAbstractItemModel* pModel = model();
    return pModel ? pModel->rowCount() : 0;
}

QList<int> AccessibleTable::selectedColumns() const {
    return {};
}

QList<int> AccessibleTable::selectedRows() const {
    QList<int> rows;
    const QItemSelectionModel* pSelection = selectionModel();
    if (!pSelection) {
        return rows;
    }
    const int count = rowCount();
    const QModelIndexList selected = pSelection->selectedIndexes();
    for (const QModelIndex& index : selected) {
        if (index.row() < count && !rows.contains(index.row())) {
            rows.append(index.row());
        }
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

bool AccessibleTable::isColumnSelected(int column) const {
    Q_UNUSED(column);
    return false;
}

bool AccessibleTable::isRowSelected(int row) const {
    const QItemSelectionModel* pSelection = selectionModel();
    return pSelection && pSelection->isRowSelected(row, QModelIndex());
}

bool AccessibleTable::selectRow(int row) {
    QItemSelectionModel* pSelection = selectionModel();
    QAbstractItemModel* pModel = model();
    if (!pSelection || !pModel || row < 0 || row >= rowCount()) {
        return false;
    }
// Current as well as selected: the library acts on the current row, and moving it
    // is what raises the focus event a screen reader follows.
    pSelection->setCurrentIndex(pModel->index(row, 0),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    return true;
}

bool AccessibleTable::selectColumn(int column) {
    Q_UNUSED(column);
    return false;
}

bool AccessibleTable::unselectRow(int row) {
    QItemSelectionModel* pSelection = selectionModel();
    QAbstractItemModel* pModel = model();
    if (!pSelection || !pModel || row < 0 || row >= rowCount()) {
        return false;
    }
    pSelection->select(pModel->index(row, 0),
            QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    return true;
}

bool AccessibleTable::unselectColumn(int column) {
    Q_UNUSED(column);
    return false;
}

void AccessibleTable::modelChange(QAccessibleTableModelChangeEvent* pEvent) {
    Q_UNUSED(pEvent);
    clearCells();
}

QAccessibleInterface* accessibleTableFactory(const QString& className, QObject* pObject) {
    Q_UNUSED(className);
    auto* pView = qobject_cast<QQuickTableView*>(pObject);
    if (pView && pView->property(kAccessibleTableProperty).toBool()) {
        return new AccessibleTable(pView);
    }
    return nullptr;
}

} // namespace qml
} // namespace mixxx
