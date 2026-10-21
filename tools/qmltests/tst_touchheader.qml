// Touch on a HorizontalHeaderView with movableColumns/resizableColumns, using the same
// two DragHandlers as res/qml/Library/TrackList.qml. Checks that a finger can resize a
// column from its right edge and move a column by dragging it, i.e. that Qt's own
// header handlers and flicking do not take the touch away. With the two handlers
// disabled, the move and resize tests fail: that is the bug (Qt finds the column edge
// from hover, and a finger never hovers). Keep the handlers here in step with
// TrackList.qml. Run offscreen with the deps bundle's qmltestrunner:
//   QT_QPA_PLATFORM=offscreen qmltestrunner -input tools/qmltests/tst_touchheader.qml
import QtQuick
import QtQuick.Controls
import QtTest
import Qt.labs.qmlmodels

Item {
    id: root

    width: 800
    height: 300

    property var visualColumnOrder: [0, 1, 2, 3]

    function visualIndexForColumn(logicalIndex) {
        return root.visualColumnOrder.indexOf(logicalIndex);
    }
    function visualIndexAtX(x) {
        let right = 0;
        for (let visualIndex = 0; visualIndex < root.visualColumnOrder.length; ++visualIndex) {
            right += view.columnWidth(root.visualColumnOrder[visualIndex]) + view.columnSpacing;
            if (x < right) {
                return visualIndex;
            }
        }
        return root.visualColumnOrder.length - 1;
    }

    HorizontalHeaderView {
        id: horizontalHeader

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        movableColumns: true
        resizableColumns: true
        syncView: view

        delegate: Item {
            id: column

            required property string display
            required property int index

            implicitHeight: 30
            implicitWidth: 100
            opacity: touchMove.active ? 0.7 : 1
            z: touchMove.active ? 2 : 0
            transform: Translate {
                x: touchMove.active ? touchMove.activeTranslation.x : 0
            }

            DragHandler {
                id: touchMove

                // activeTranslation is back to 0 by the time active turns false.
                property real draggedX: 0
                // A grab taken away (a popup opening, a touch cancel) is no drop.
                property bool cancelled: false

                acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                target: null
                yAxis.enabled: false

                onActiveTranslationChanged: {
                    if (active) {
                        draggedX = activeTranslation.x;
                    }
                }
                onCanceled: cancelled = true
                onActiveChanged: {
                    if (active) {
                        draggedX = 0;
                        cancelled = false;
                        return;
                    }
                    if (cancelled) {
                        return;
                    }
                    const source = root.visualIndexForColumn(column.index);
                    const destination = root.visualIndexAtX(column.x + column.width / 2 + draggedX);
                    if (source >= 0 && destination >= 0 && destination !== source) {
                        view.moveColumn(source, destination);
                    }
                }
            }
            Item {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.top: parent.top
                width: Math.min(24, parent.width / 3)
                z: 1

                DragHandler {
                    id: touchResize

                    property real startWidth: 0

                    acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                    grabPermissions: PointerHandler.CanTakeOverFromAnything
                    target: null
                    yAxis.enabled: false

                    onActiveChanged: {
                        if (active) {
                            startWidth = view.columnWidth(column.index);
                        }
                    }
                    onActiveTranslationChanged: {
                        if (active) {
                            view.setColumnWidth(column.index, Math.max(30, startWidth + activeTranslation.x));
                        }
                    }
                }
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton | Qt.RightButton
            }
            Text {
                anchors.fill: parent
                text: column.display
            }
        }
    }
    TableView {
        id: view

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: horizontalHeader.bottom
        columnWidthProvider: function (c) {
            const w = explicitColumnWidth(c);
            return w >= 0 ? w : 150;
        }
        model: TableModel {
            TableModelColumn { display: "a" }
            TableModelColumn { display: "b" }
            TableModelColumn { display: "c" }
            TableModelColumn { display: "d" }
            rows: [{ "a": "1", "b": "2", "c": "3", "d": "4" }, { "a": "5", "b": "6", "c": "7", "d": "8" }]
        }
        delegate: Text {
            required property string display
            text: display
        }

        onColumnMoved: (logicalIndex, oldVisualIndex, newVisualIndex) => {
            // Same bookkeeping as TrackList.handleColumnMoved.
            const order = root.visualColumnOrder.slice();
            const currentIndex = order.indexOf(logicalIndex);
            order.splice(currentIndex, 1);
            order.splice(newVisualIndex, 0, logicalIndex);
            root.visualColumnOrder = order;
        }
    }

    TestCase {
        name: "TouchHeader"
        when: windowShown

        function drag(fromX, toX) {
            const y = horizontalHeader.height / 2;
            const t = touchEvent(root);
            t.press(0, root, fromX, y).commit();
            for (let step = 1; step <= 10; ++step) {
                wait(16);
                t.move(0, root, fromX + (toX - fromX) * step / 10, y).commit();
            }
            wait(16);
            t.release(0, root, toX, y).commit();
            wait(50);
        }

        function init() {
            view.clearColumnReordering();
            view.clearColumnWidths();
            root.visualColumnOrder = [0, 1, 2, 3];
            view.forceLayout();
            wait(20);
        }

        function test_resizeByTouch() {
            compare(view.columnWidth(0), 150);
            // Right edge of column 0 is at 150; the grip is the 24 px inside it.
            drag(140, 220);
            verify(view.columnWidth(0) > 200, "width after touch resize: " + view.columnWidth(0));
            compare(root.visualColumnOrder, [0, 1, 2, 3], "a resize must not move the column");
        }

        function test_moveByTouch() {
            // Middle of column 0 (75) dragged into column 2 (300..450).
            drag(60, 360);
            compare(root.visualColumnOrder, [1, 2, 0, 3]);
            compare(view.columnWidth(0), 150, "a move must not resize");
        }

        function test_tapStillSorts() {
            // A touch tap is not a drag: no move, no resize.
            const t = touchEvent(root);
            t.press(0, root, 60, 15).commit();
            wait(30);
            t.release(0, root, 60, 15).commit();
            wait(50);
            compare(root.visualColumnOrder, [0, 1, 2, 3]);
            compare(view.columnWidth(0), 150);
        }
    }
}
