import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import "../Theme"

Rectangle {
    id: root

    required property var model
    property int columnCount: 0
    property var defaultColumnLayout: []
    property var initializedModel: null
    property bool restoringLayout: false
    property var visualColumnOrder: []

    readonly property string layoutSettingName: "qml_header_state_v1"

    // Floor for a user-dragged column width. Hidden and auto-hidden columns
    // bypass this deliberately -- they return 0 earlier in columnWidthProvider.
    readonly property int minimumColumnWidth: 40

    component ColumnMenuItem: MenuItem {
        id: menuItem

        implicitHeight: 30
        implicitWidth: 220
        leftPadding: checkable ? 28 : 10
        rightPadding: subMenu ? 28 : 10

        arrow: Text {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            color: menuItem.enabled ? Theme.deckTextColor : Theme.midGray
            font.family: Theme.fontFamily
            font.pixelSize: 11
            text: "▶"
            visible: menuItem.subMenu
        }
        background: Rectangle {
            color: menuItem.highlighted && menuItem.enabled
                    ? Qt.rgba(0.004, 0.863, 0.988, 0.18)
                    : "transparent"
            radius: 3
        }
        contentItem: Text {
            color: menuItem.enabled ? Theme.deckTextColor : Theme.midGray
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: 12
            text: menuItem.text
            verticalAlignment: Text.AlignVCenter
        }
        indicator: Text {
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.blue
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 14
            text: "✓"
            visible: menuItem.checkable && menuItem.checked
        }
    }
    component ColumnMenuSeparator: MenuSeparator {
        implicitHeight: 7

        contentItem: Rectangle {
            color: Theme.midGray
            implicitHeight: 1
            opacity: 0.5
        }
    }
    component ColumnMenu: Menu {
        delegate: ColumnMenuItem {
        }
        padding: 4

        background: Rectangle {
            border.color: Theme.midGray
            border.width: 1
            color: Theme.darkGray2
            implicitWidth: 220
            radius: 4
        }
    }

    function captureDefaultLayout() {
        const result = [];
        if (!root.model) {
            return result;
        }
        for (let index = 0; index < root.model.columns.length; ++index) {
            const column = root.model.columns[index];
            result.push({
                "id": column.layoutId,
                "preferredWidth": column.preferredWidth,
                "hidden": column.hidden
            });
        }
        return result;
    }
    function columnMenuLabel(column) {
        return column.label.length > 0 ? column.label : qsTr("Cover");
    }
    function initializeVisualColumnOrder() {
        const order = [];
        for (let index = 0; index < root.columnCount; ++index) {
            order.push(index);
        }
        root.visualColumnOrder = order;
    }
    function visualIndexForColumn(logicalIndex) {
        return root.visualColumnOrder.indexOf(logicalIndex);
    }
    function handleColumnMoved(logicalIndex, oldVisualIndex, newVisualIndex) {
        const order = root.visualColumnOrder.slice();
        const currentIndex = order.indexOf(logicalIndex);
        if (currentIndex < 0) {
            return;
        }
        order.splice(currentIndex, 1);
        order.splice(newVisualIndex, 0, logicalIndex);
        root.visualColumnOrder = order;
        root.saveLayout();
    }
    function captureExplicitColumnWidths() {
        if (root.restoringLayout || !root.model) {
            return;
        }
        let changed = false;
        for (let index = 0; index < root.model.columns.length; ++index) {
            const explicitWidth = view.explicitColumnWidth(index);
            if (explicitWidth >= 0 && Math.abs(root.model.columns[index].preferredWidth - explicitWidth) >= 0.5) {
                root.model.columns[index].preferredWidth = explicitWidth;
                changed = true;
            }
        }
        if (changed) {
            widthSaveTimer.restart();
        }
    }
    function visibleColumnCount() {
        let count = 0;
        if (!root.model) {
            return count;
        }
        for (let index = 0; index < root.model.columns.length; ++index) {
            if (!root.model.columns[index].hidden) {
                ++count;
            }
        }
        return count;
    }
    function saveLayout() {
        if (root.restoringLayout || !root.model) {
            return;
        }
        const order = [];
        const columns = {};
        for (let visualIndex = 0; visualIndex < root.visualColumnOrder.length; ++visualIndex) {
            order.push(root.model.columns[root.visualColumnOrder[visualIndex]].layoutId);
        }
        for (let index = 0; index < root.model.columns.length; ++index) {
            const column = root.model.columns[index];
            columns[column.layoutId] = {
                "preferredWidth": column.preferredWidth,
                "hidden": column.hidden
            };
        }
        const sortColumn = horizontalHeader.sortingColumn;
        const state = {
            "version": 1,
            "order": order,
            "columns": columns,
            "sort": sortColumn >= 0 ? {
                "id": root.model.columns[sortColumn].layoutId,
                "order": horizontalHeader.sortingOrder
            } : null
        };
        root.model.setModelSetting(root.layoutSettingName, JSON.stringify(state));
    }
    function restoreLayout() {
        if (!root.model || root.initializedModel === root.model) {
            return;
        }
        root.initializedModel = root.model;
        root.restoringLayout = true;
        view.clearColumnReordering();
        view.clearColumnWidths();
        root.columnCount = root.model.columns.length;
        root.initializeVisualColumnOrder();
        root.defaultColumnLayout = root.captureDefaultLayout();

        let state = null;
        const storedState = root.model.getModelSetting(root.layoutSettingName);
        if (storedState.length > 0) {
            try {
                state = JSON.parse(storedState);
            } catch (error) {
                console.warn("Ignoring invalid QML library layout:", error);
            }
        }

        if (state && state.version === 1) {
            if (Array.isArray(state.order)) {
                for (let target = 0; target < state.order.length; ++target) {
                    const logicalIndex = root.model.columnIndexByLayoutId(state.order[target]);
                    const source = root.visualIndexForColumn(logicalIndex);
                    if (source >= 0 && source !== target) {
                        view.moveColumn(source, target);
                    }
                }
            }
            if (state.columns) {
                for (let index = 0; index < root.model.columns.length; ++index) {
                    const column = root.model.columns[index];
                    const savedColumn = state.columns[column.layoutId];
                    if (!savedColumn) {
                        continue;
                    }
                    if (typeof savedColumn.preferredWidth === "number") {
                        column.preferredWidth = savedColumn.preferredWidth;
                    }
                    if (typeof savedColumn.hidden === "boolean") {
                        column.hidden = savedColumn.hidden;
                    }
                }
            }
            if (state.sort && typeof state.sort.id === "string") {
                const sortColumn = root.model.columnIndexByLayoutId(state.sort.id);
                if (sortColumn >= 0) {
                    horizontalHeader.sortingColumn = sortColumn;
                    horizontalHeader.sortingOrder = state.sort.order === Qt.DescendingOrder ? Qt.DescendingOrder : Qt.AscendingOrder;
                    root.model.sort(sortColumn, horizontalHeader.sortingOrder);
                }
            }
        }

        if (root.visibleColumnCount() === 0) {
            for (let index = 0; index < root.model.columns.length; ++index) {
                root.model.columns[index].hidden = false;
            }
        }
        root.restoringLayout = false;
        view.updateColumnSize();
        view.forceLayout();
    }
    function setColumnVisible(index, visible) {
        if (!root.model || index < 0 || index >= root.model.columns.length) {
            return;
        }
        if (!visible && root.visibleColumnCount() <= 1) {
            return;
        }
        root.model.columns[index].hidden = !visible;
        view.updateColumnSize();
        view.forceLayout();
        root.saveLayout();
    }
    function moveColumn(logicalIndex, direction) {
        const source = root.visualIndexForColumn(logicalIndex);
        const destination = source + direction;
        if (source < 0 || destination < 0 || destination >= root.columnCount) {
            return;
        }
        view.moveColumn(source, destination);
    }
    function resetColumnLayout() {
        if (!root.model || root.defaultColumnLayout.length === 0) {
            return;
        }
        root.restoringLayout = true;
        view.clearColumnReordering();
        view.clearColumnWidths();
        root.initializeVisualColumnOrder();
        for (let index = 0; index < root.defaultColumnLayout.length; ++index) {
            const savedColumn = root.defaultColumnLayout[index];
            const columnIndex = root.model.columnIndexByLayoutId(savedColumn.id);
            if (columnIndex >= 0) {
                root.model.columns[columnIndex].preferredWidth = savedColumn.preferredWidth;
                root.model.columns[columnIndex].hidden = savedColumn.hidden;
            }
        }
        view.updateColumnSize();
        view.forceLayout();
        root.restoringLayout = false;
        root.saveLayout();
    }

    color: Theme.darkGray

    Timer {
        id: widthSaveTimer

        interval: 250
        repeat: false

        onTriggered: root.saveLayout()
    }

    LibraryComponent.Control {
        id: libraryControl

        onFocusWidgetChanged: {
            switch (focusWidget) {
            case Skin.FocusedWidgetControl.WidgetKind.LibraryView:
                view.forceActiveFocus();
                break;
            }
        }
        onLoadSelectedTrack: (group, play) => {
            view.loadSelectedTrack(group, play);
        }
        onLoadSelectedTrackIntoNextAvailableDeck: play => {
            view.loadSelectedTrackIntoNextAvailableDeck(play);
        }
        onMoveVertical: offset => {
            view.selectionModel.moveSelectionVertical(offset);
        }
    }
    Rectangle {
        id: headerBackground

        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.toolbarBackgroundColor
        height: horizontalHeader.height + 2
        radius: 3

        // The header view only has delegates where columns actually are, so a
        // right-click past the last column landed on nothing and no menu opened.
        // Handle it on the background strip underneath: the menu opens with no
        // column context (columnIndex -1), which leaves "Move column left/right"
        // disabled -- visualIndexForColumn(-1) returns -1 -- while the Columns
        // visibility submenu stays usable, which is the point of asking here.
        TapHandler {
            acceptedButtons: Qt.RightButton

            onTapped: {
                headerMenu.columnIndex = -1;
                headerMenu.popup();
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Theme.blue
            height: 1
            opacity: 0.35
        }
    }
    HorizontalHeaderView {
        id: horizontalHeader

        property int sortingColumn: -1
        property var sortingOrder: Qt.DescendingOrder

        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: parent.top
        movableColumns: true
        resizableColumns: true
        syncView: view
        z: 1

        delegate: Item {
            id: column

            required property string display
            required property int index

            implicitHeight: columnName.contentHeight + 5
            implicitWidth: columnName.contentWidth + 5

            TapHandler {
                id: columnMouseHandler

                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onTapped: (eventPoint, button) => {
                    if (button === Qt.RightButton) {
                        headerMenu.columnIndex = index;
                        headerMenu.popup();
                        return;
                    }
                    if (horizontalHeader.sortingColumn == index) {
                        horizontalHeader.sortingOrder = horizontalHeader.sortingOrder == Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder;
                    } else {
                        horizontalHeader.sortingColumn = index;
                        horizontalHeader.sortingOrder = Qt.AscendingOrder;
                    }
                    view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
                    root.saveLayout();
                }
                onLongPressed: eventPoint => {
                    headerMenu.columnIndex = index;
                    headerMenu.popup();
                }
            }
            Text {
                id: columnName

                anchors.fill: parent
                anchors.leftMargin: 15
                color: Theme.textColor
                elide: Text.ElideRight
                font.capitalization: Font.Capitalize
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignLeft
                text: display
                verticalAlignment: Text.AlignVCenter
            }
            Item {
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    leftMargin: 5
                    top: parent.top
                }
                Label {
                    id: sortIndicator

                    anchors.centerIn: parent
                    color: "red"
                    elide: Text.ElideRight
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.buttonFontPixelSize
                    horizontalAlignment: Text.AlignRight
                    rotation: horizontalHeader.sortingOrder == Qt.AscendingOrder ? 90 : -90
                    text: "▶"
                    verticalAlignment: Text.AlignVCenter
                    visible: horizontalHeader.sortingColumn == index
                }
            }
            Rectangle {
                id: columnResizer

                // Was Theme.darkGray2 -- but toolbarBackgroundColor IS darkGray2,
                // so the divider was drawn in the header's own colour at 1.00:1
                // contrast and no column separators were visible. Inset top and
                // bottom so it reads as a separator rather than a full-height rule.
                color: Theme.midGray
                opacity: 0.45
                width: 1

                anchors {
                    bottom: parent.bottom
                    bottomMargin: 4
                    right: parent.right
                    top: parent.top
                    topMargin: 4
                }
            }
        }
    }
    ColumnMenu {
        id: headerMenu

        property int columnIndex: -1

        ColumnMenuItem {
            enabled: root.visualIndexForColumn(headerMenu.columnIndex) > 0
            text: qsTr("Move column left")

            onTriggered: root.moveColumn(headerMenu.columnIndex, -1)
        }
        ColumnMenuItem {
            enabled: {
                const visualIndex = root.visualIndexForColumn(headerMenu.columnIndex);
                return visualIndex >= 0 && visualIndex < root.columnCount - 1;
            }
            text: qsTr("Move column right")

            onTriggered: root.moveColumn(headerMenu.columnIndex, 1)
        }
        ColumnMenuSeparator {
        }
        ColumnMenu {
            id: columnsMenu

            property var columnSnapshot: []

            function refresh() {
                const snapshot = [];
                const visibleCount = root.visibleColumnCount();
                if (root.model) {
                    for (let index = 0; index < root.model.columns.length; ++index) {
                        const column = root.model.columns[index];
                        snapshot.push({
                            "index": index,
                            "label": root.columnMenuLabel(column),
                            "visible": !column.hidden,
                            "canToggle": column.hidden || visibleCount > 1
                        });
                    }
                }
                columnSnapshot = snapshot;
            }

            title: qsTr("Visible columns")

            onAboutToShow: refresh()

            Instantiator {
                model: columnsMenu.columnSnapshot

                delegate: ColumnMenuItem {
                    required property var modelData

                    checkable: true
                    checked: modelData.visible
                    enabled: modelData.canToggle
                    text: modelData.label

                    onTriggered: root.setColumnVisible(modelData.index, checked)
                }

                onObjectAdded: (index, object) => columnsMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => columnsMenu.removeItem(object)
            }
        }
        ColumnMenuSeparator {
        }
        ColumnMenuItem {
            text: qsTr("Reset column layout")

            onTriggered: root.resetColumnLayout()
        }
    }
    TableView {
        id: view

        property int dynamicColumnCount: 0
        property int usedWidth: 0

        function loadSelectedTrack(group, play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.getPlayer(group).loadTrackFromLocationUrl(urls[0], play);
        }
        function loadSelectedTrackIntoNextAvailableDeck(play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(urls[0], play);
        }
        function updateColumnSize() {
            const oldUsedWidth = usedWidth;
            const oldDynamicColumnCount = dynamicColumnCount;
            usedWidth = 0;
            dynamicColumnCount = 0;
            if (model == null) {
                return;
            }
            for (let c = 0; c < model.columns.length; c++) {
                if (model.columns[c].hidden || model.columns[c].autoHideWidth > view.width) {
                    continue;
                } else if (model.columns[c].preferredWidth > 0) {
                    usedWidth += model.columns[c].preferredWidth;
                } else {
                    dynamicColumnCount += model.columns[c].fillSpan || 1;
                }
            }
            return oldDynamicColumnCount != dynamicColumnCount || oldUsedWidth != usedWidth;
        }

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: horizontalHeader.bottom
        clip: true
        columnWidthProvider: function (column) {
            const columnDef = view.model.columns[column];
            if (columnDef.hidden) {
                return 0;
            }
            if (columnDef.autoHideWidth > 0 && columnDef.autoHideWidth > view.width) {
                return 0;
            }
            const explicitWidth = view.explicitColumnWidth(column);
            if (explicitWidth >= 0) {
                // A resize drag can set an explicit width of 0, collapsing the
                // column completely -- with no handle left to grab, and no way
                // to bring it back. Hiding a column is what the header menu is
                // for; a drag must never make a visible column unreachable.
                return Math.max(explicitWidth, root.minimumColumnWidth);
            }
            if (columnDef.preferredWidth >= 0) {
                return columnDef.preferredWidth;
            }
            const span = columnDef.fillSpan || 1;
            return span * (view.width - view.usedWidth) / view.dynamicColumnCount;
        }
        keyNavigationEnabled: false
        model: root.model
        pointerNavigationEnabled: false
        reuseItems: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
        }
        delegate: Item {
            id: item

            required property url cover_art
            required property color decoration
            required property var display
            required property string file_url
            required property int row
            required property bool selected
            required property var track

            implicitHeight: Mixxx.Config.libraryRowHeight

            Loader {
                id: loader

                property var capabilities: root.model ? root.model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
                property url cover_art: item.cover_art
                property color decoration: item.decoration
                property var display: item.display
                property url file_url: item.file_url
                property int row: item.row
                property bool selected: item.selected
                property var tableView: view
                property var track: item.track

                anchors.fill: parent
                focus: true
                sourceComponent: delegate

                onLoaded:
                // Workaround needed for WaveformOverview column to load the data
                //     if (track)
                //         Mixxx.Library.analyze(track)
                {}
            }
            // Workaround needed for WaveformOverview column to load the data
            // TableView.onReused: {
            //     if (track)
            //         Mixxx.Library.analyze(track)
            // }
        }
        selectionModel: ItemSelectionModel {
            function moveSelectionVertical(value) {
                if (value == 0)
                    return;

                const selected = this.selectedIndexes;
                const oldRow = (selected.length == 0) ? 0 : selected[0].row;
                this.selectRow(oldRow + value);
            }
            function selectRow(row) {
                const rowCount = this.model.rowCount();
                if (rowCount == 0) {
                    this.clear();
                    return;
                }
                const newRow = Mixxx.MathUtils.positiveModulo(row, rowCount);
                this.select(this.model.index(newRow, 0), ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear | ItemSelectionModel.Current);
            }
            function selectedTrackUrls() {
                return this.selectedIndexes.map(index => {
                    return this.model.getUrl(index.row);
                });
            }

            model: view.model
        }

        Component.onCompleted: Qt.callLater(root.restoreLayout)
        Keys.onDownPressed: this.selectionModel.moveSelectionVertical(1)
        Keys.onEnterPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onReturnPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onUpPressed: this.selectionModel.moveSelectionVertical(-1)
        onModelChanged: Qt.callLater(root.restoreLayout)
        onColumnMoved: (logicalIndex, oldVisualIndex, newVisualIndex) => root.handleColumnMoved(logicalIndex, oldVisualIndex, newVisualIndex)
        onLayoutChanged: root.captureExplicitColumnWidths()
        onWidthChanged: {
            if (view.updateColumnSize()) {
                // forceLayout is costly - only invoke if there was a change in the column layouts
                view.forceLayout();
            }
        }
    }
}
