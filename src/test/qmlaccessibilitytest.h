#pragma once

#include <QList>
#include <QObject>
#include <QQmlListProperty>
#include <QStandardItem>
#include <QStandardItemModel>

/// A table model shaped like the library's: text cells, header labels, and a
/// `columns` list whose entries say whether a column is hidden.
class ColumnsModel : public QStandardItemModel {
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> columns READ columns CONSTANT)
  public:
    ColumnsModel(int rows, int columns)
            : QStandardItemModel(rows, columns) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < columns; ++c) {
                setItem(r, c, new QStandardItem(QStringLiteral("r%1c%2").arg(r).arg(c)));
            }
        }
        for (int c = 0; c < columns; ++c) {
            setHorizontalHeaderItem(c, new QStandardItem(QStringLiteral("Header %1").arg(c)));
            auto* pDefinition = new QObject(this);
            pDefinition->setProperty("hidden", false);
            m_columns.append(pDefinition);
        }
    }
    QQmlListProperty<QObject> columns() {
        return QQmlListProperty<QObject>(this, &m_columns);
    }
    QList<QObject*> m_columns;
};
