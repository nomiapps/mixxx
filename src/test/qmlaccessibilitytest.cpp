#include <gtest/gtest.h>

#include <QAccessible>
#include <QColor>
#include <QCoreApplication>
#include <QItemSelectionModel>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardItemModel>
#include <QThread>
#include <QUrl>
#include <memory>

#include "control/controlpotmeter.h"
#include "qml/qmlaccessibletable.h"
#include "qml/qmlconfigproxy.h"
#include "test/mixxxtest.h"
#include "test/qmlaccessibilitytest.h"

// What a screen reader gets from the New UI's continuous controls, asked through the
// same QAccessible interfaces Qt's UI Automation bridge serves on Windows. The widgets
// are the real skin files bound to real controls; only the window is offscreen.
namespace {

class QmlAccessibilityTest : public MixxxTest {
  protected:
    void SetUp() override {
        mixxx::qml::QmlConfigProxy::registerUserSettings(config());
        m_engine.addImportPath(QStringLiteral(":/mixxx.org/imports"));
        m_engine.addImportPath(QStringLiteral(RESOURCE_FOLDER "/qml"));
        QAccessible::setActive(true);
    }

    void TearDown() override {
        m_item.reset();
        QAccessible::setActive(false);
    }

    // Declares a skin type in QML the way the skin itself does, bound to a control.
    // Initial properties from C++ cannot reach ControlKnob's group and key: they are
    // aliases of the ControlProxy's required properties, which Qt only accepts in QML.
    QQuickItem* load(const QString& type, const QString& group, const QString& key) {
        // Knobs require an arc colour; faders have none.
        const QString colour = type.contains(QStringLiteral("Knob"))
                ? QStringLiteral("; color: \"white\"")
                : QString();
        const QString qml = QStringLiteral(
                "import \".\" as Skin\n"
                "Skin.%1 { group: \"%2\"; key: \"%3\"%4 }\n")
                                    .arg(type, group, key, colour);
        QQmlComponent component(&m_engine);
        component.setData(qml.toUtf8(),
                QUrl::fromLocalFile(QStringLiteral(RESOURCE_FOLDER "/qml/AccessibilityTest.qml")));
        std::unique_ptr<QObject> object(component.create());
        if (!object) {
            ADD_FAILURE() << qPrintable(component.errorString());
            return nullptr;
        }
        auto* item = qobject_cast<QQuickItem*>(object.release());
        item->setParentItem(m_window.contentItem());
        item->setSize(QSizeF(60, 200));
        m_item.reset(item);
        settle();
        return item;
    }

    static void settle() {
        for (int i = 0; i < 5; ++i) {
            QCoreApplication::processEvents();
        }
    }

    static QAccessibleInterface* accessible(QQuickItem* item) {
        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(item);
        EXPECT_NE(iface, nullptr);
        return iface;
    }

    // The behaviour every value control must share: readable, settable, steppable,
    // and still following its control after a screen reader has set it.
    void expectReadableAndSettable(QQuickItem* item, ControlPotmeter* pControl) {
        QAccessibleInterface* iface = accessible(item);
        ASSERT_NE(iface, nullptr);
        QAccessibleValueInterface* value = iface->valueInterface();
        ASSERT_NE(value, nullptr) << "no value interface for role " << iface->role();
        EXPECT_DOUBLE_EQ(value->minimumValue().toDouble(), 0.0);
        EXPECT_DOUBLE_EQ(value->maximumValue().toDouble(), 1.0);

        pControl->set(0.5);
        settle();
        EXPECT_NEAR(value->currentValue().toDouble(), 0.5, 1e-9) << "the value follows the control";

        value->setCurrentValue(0.25);
        settle();
        EXPECT_NEAR(pControl->get(), 0.25, 1e-9) << "a screen reader's value reaches the control";

        pControl->set(0.75);
        settle();
        EXPECT_NEAR(value->currentValue().toDouble(), 0.75, 1e-9)
                << "the widget still follows the control after a screen reader set it";

        QAccessibleActionInterface* actions = iface->actionInterface();
        ASSERT_NE(actions, nullptr);
        const QStringList names = actions->actionNames();
        ASSERT_TRUE(names.contains(QAccessibleActionInterface::increaseAction()))
                << qPrintable(names.join(u','));
        ASSERT_TRUE(names.contains(QAccessibleActionInterface::decreaseAction()));
        actions->doAction(QAccessibleActionInterface::increaseAction());
        settle();
        EXPECT_GT(pControl->get(), 0.75);
        EXPECT_LT(pControl->get(), 0.76);
        const double raised = pControl->get();
        actions->doAction(QAccessibleActionInterface::decreaseAction());
        actions->doAction(QAccessibleActionInterface::decreaseAction());
        settle();
        EXPECT_LT(pControl->get(), raised);
    }

    QQmlEngine m_engine;
    QQuickWindow m_window;
    std::unique_ptr<QQuickItem> m_item;
};

TEST_F(QmlAccessibilityTest, KnobIsANamedDialAScreenReaderCanReadAndSet) {
    ControlPotmeter control(ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("volume")), 0.0, 1.0);
    QQuickItem* knob = load(QStringLiteral("ControlKnob"), QStringLiteral("[Channel1]"), QStringLiteral("volume"));
    ASSERT_NE(knob, nullptr);
    QAccessibleInterface* iface = accessible(knob);
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->role(), QAccessible::Dial);
    EXPECT_EQ(iface->text(QAccessible::Name), QStringLiteral("Volume, deck 1"));
    expectReadableAndSettable(knob, &control);
}

TEST_F(QmlAccessibilityTest, MiniKnobIsNamedAndSettableToo) {
    ControlPotmeter control(ConfigKey(QStringLiteral("[Channel2]"), QStringLiteral("volume")), 0.0, 1.0);
    QQuickItem* knob = load(QStringLiteral("ControlMiniKnob"), QStringLiteral("[Channel2]"), QStringLiteral("volume"));
    ASSERT_NE(knob, nullptr);
    QAccessibleInterface* iface = accessible(knob);
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->text(QAccessible::Name), QStringLiteral("Volume, deck 2"));
    expectReadableAndSettable(knob, &control);
}

TEST_F(QmlAccessibilityTest, FaderIsANamedSliderAScreenReaderCanReadAndSet) {
    ControlPotmeter control(ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("volume")), 0.0, 1.0);
    QQuickItem* fader = load(QStringLiteral("ControlFader"), QStringLiteral("[Channel1]"), QStringLiteral("volume"));
    ASSERT_NE(fader, nullptr);
    QAccessibleInterface* iface = accessible(fader);
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->role(), QAccessible::Slider);
    EXPECT_EQ(iface->text(QAccessible::Name), QStringLiteral("Volume, deck 1"));
    expectReadableAndSettable(fader, &control);
}

class QmlAccessibleTableTest : public QmlAccessibilityTest {
  protected:
    void SetUp() override {
        QmlAccessibilityTest::SetUp();
        QAccessible::installFactory(mixxx::qml::accessibleTableFactory);
    }
    void TearDown() override {
        QmlAccessibilityTest::TearDown();
        QAccessible::removeFactory(mixxx::qml::accessibleTableFactory);
    }

    // A plain TableView marked the way TrackList.qml marks the library's.
    QQuickItem* table(ColumnsModel* pModel) {
        QQmlComponent component(&m_engine);
        component.setData(QByteArrayLiteral(
                                  "import QtQuick\n"
                                  "import QtQml.Models\n"
                                  "TableView {\n"
                                  "    id: view\n"
                                  "    readonly property bool mixxxAccessibleTable: true\n"
                                  "    width: 240; height: 100\n"
                                  "    selectionModel: ItemSelectionModel { model: view.model }\n"
                                  "    delegate: Rectangle { implicitWidth: 80; implicitHeight: 20 }\n"
                                  "}\n"),
                QUrl());
        std::unique_ptr<QObject> object(component.create());
        if (!object) {
            ADD_FAILURE() << qPrintable(component.errorString());
            return nullptr;
        }
        auto* pView = qobject_cast<QQuickItem*>(object.release());
        pView->setProperty("model", QVariant::fromValue<QObject*>(pModel));
        pView->setParentItem(m_window.contentItem());
        m_item.reset(pView);
        m_window.resize(240, 100);
        m_window.show();
        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }
        return pView;
    }
};

TEST_F(QmlAccessibleTableTest, ALibraryTableIsATableWithRowsColumnsAndHeaders) {
    ColumnsModel model(30, 3);
    QQuickItem* pView = table(&model);
    ASSERT_NE(pView, nullptr);
    QAccessibleInterface* iface = accessible(pView);
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->role(), QAccessible::Table);
    QAccessibleTableInterface* pTable = iface->tableInterface();
    ASSERT_NE(pTable, nullptr);
    EXPECT_EQ(pTable->rowCount(), 30);
    EXPECT_EQ(pTable->columnCount(), 3);
    EXPECT_EQ(pTable->columnDescription(2), QStringLiteral("Header 2"));

    QAccessibleInterface* pCell = pTable->cellAt(2, 1);
    ASSERT_NE(pCell, nullptr);
    EXPECT_EQ(pCell->role(), QAccessible::Cell);
    EXPECT_EQ(pCell->text(QAccessible::Name), QStringLiteral("r2c1"));
    EXPECT_EQ(pCell->parent(), iface);
    QAccessibleTableCellInterface* pCellInfo = pCell->tableCellInterface();
    ASSERT_NE(pCellInfo, nullptr);
    EXPECT_EQ(pCellInfo->rowIndex(), 2);
    EXPECT_EQ(pCellInfo->columnIndex(), 1);
    ASSERT_EQ(pCellInfo->columnHeaderCells().size(), 1);
    EXPECT_EQ(pCellInfo->columnHeaderCells().first()->text(QAccessible::Name),
            QStringLiteral("Header 1"));
    EXPECT_EQ(pTable->cellAt(2, 1), pCell) << "the same cell comes back, not a new one";

    // Headers first, then the cells row by row, as a QTableView orders its children.
    EXPECT_EQ(iface->childCount(), 3 + 30 * 3);
    EXPECT_EQ(iface->child(0)->role(), QAccessible::ColumnHeader);
    EXPECT_EQ(iface->indexOfChild(pCell), 3 + 2 * 3 + 1);
    EXPECT_EQ(iface->child(3 + 2 * 3 + 1), pCell);
}

TEST_F(QmlAccessibleTableTest, RowsOutOfViewAreReachableButOffscreen) {
    ColumnsModel model(30, 3);
    QQuickItem* pView = table(&model);
    ASSERT_NE(pView, nullptr);
    QAccessibleTableInterface* pTable = accessible(pView)->tableInterface();
    ASSERT_NE(pTable, nullptr);
    QAccessibleInterface* pVisible = pTable->cellAt(0, 0);
    QAccessibleInterface* pFar = pTable->cellAt(29, 0);
    ASSERT_NE(pVisible, nullptr);
    ASSERT_NE(pFar, nullptr);
    EXPECT_EQ(pFar->text(QAccessible::Name), QStringLiteral("r29c0"));
    EXPECT_TRUE(pFar->state().offscreen);
    EXPECT_FALSE(pVisible->state().offscreen) << "the first row has a delegate in a 100 px view";
    EXPECT_FALSE(pVisible->rect().isEmpty());
}

TEST_F(QmlAccessibleTableTest, HiddenColumnsAreLeftOut) {
    ColumnsModel model(5, 3);
    QQuickItem* pView = table(&model);
    ASSERT_NE(pView, nullptr);
    QAccessibleTableInterface* pTable = accessible(pView)->tableInterface();
    ASSERT_NE(pTable, nullptr);
    model.m_columns[1]->setProperty("hidden", true);
    EXPECT_EQ(pTable->columnCount(), 2);
    EXPECT_EQ(pTable->columnDescription(1), QStringLiteral("Header 2"));
    EXPECT_EQ(pTable->cellAt(3, 1)->text(QAccessible::Name), QStringLiteral("r3c2"));
}

TEST_F(QmlAccessibleTableTest, SelectionIsReadAndSetThroughTheTable) {
    ColumnsModel model(10, 3);
    QQuickItem* pView = table(&model);
    ASSERT_NE(pView, nullptr);
    QAccessibleTableInterface* pTable = accessible(pView)->tableInterface();
    ASSERT_NE(pTable, nullptr);
    EXPECT_TRUE(pTable->selectedRows().isEmpty());
    ASSERT_TRUE(pTable->selectRow(4));
    EXPECT_TRUE(pTable->isRowSelected(4));
    EXPECT_EQ(pTable->selectedRows(), QList<int>{4});
    EXPECT_EQ(pTable->selectedCellCount(), 3);
    EXPECT_TRUE(pTable->cellAt(4, 2)->state().selected);
    EXPECT_FALSE(pTable->cellAt(3, 2)->state().selected);
    auto* pSelection = pView->property("selectionModel").value<QItemSelectionModel*>();
    ASSERT_NE(pSelection, nullptr);
    EXPECT_EQ(pSelection->currentIndex().row(), 4) << "selecting a row makes it current";
    ASSERT_TRUE(pTable->unselectRow(4));
    EXPECT_FALSE(pTable->isRowSelected(4));
}

TEST_F(QmlAccessibleTableTest, ShrinkingTheModelDropsStaleCells) {
    ColumnsModel model(30, 3);
    QQuickItem* pView = table(&model);
    ASSERT_NE(pView, nullptr);
    QAccessibleTableInterface* pTable = accessible(pView)->tableInterface();
    ASSERT_NE(pTable, nullptr);
    ASSERT_NE(pTable->cellAt(29, 0), nullptr);
    model.setRowCount(5);
    QCoreApplication::processEvents();
    EXPECT_EQ(pTable->rowCount(), 5);
    EXPECT_EQ(pTable->cellAt(29, 0), nullptr);
    EXPECT_EQ(pTable->cellAt(4, 0)->text(QAccessible::Name), QStringLiteral("r4c0"));
}

} // namespace

#include "moc_qmlaccessibilitytest.cpp"
