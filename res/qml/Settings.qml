import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import "Theme"
import "Settings" as Settings

// The settings window wears the same chrome as the rest of the New UI: the
// main-window ground, sunken panels behind a 1px Theme.panelBorderColor hairline
// with a 4px corner, a 28px toolbar-coloured header strip with an 11px bold label
// and a blue underline, and rows that highlight in Theme.blue at 16%. The
// reference is Library/Browser.qml; the deck and mixer restyle set the values.
Popup {
    id: root

    property var activeCategory: null
    property alias activeCategoryIndex: categoryList.currentIndex
    readonly property var manager: managerItem
    property alias sections: managerItem.data

    function updateActiveCategory() {
        root.activeCategory?.deactivated();
        root.activeCategory = managerItem.data[categoryList.currentIndex] ?? null;
        root.activeCategory?.activated();
    }

    // Stated rather than inherited: this popup had no close button and no declared
    // policy, so the only way out was Escape -- and nothing on screen said so. When it
    // fills the window there is no "outside" left to click either.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // CloseOnEscape only fires while the popup holds ACTIVE FOCUS, so the policy alone
    // did nothing: Escape closed nested dialogs, which take focus themselves, but never
    // this window.
    focus: true
    horizontalPadding: 20
    verticalPadding: 20

    background: Rectangle {
        anchors.fill: parent
        border.color: Theme.panelBorderColor
        border.width: 1
        color: Theme.backgroundColor
        opacity: parent.radius < 0 ? Math.max(0.1, 1 + parent.radius / 8) : 1
        radius: 8
    }
    contentItem: Item {
        anchors.centerIn: parent
        height: parent.height - 40
        width: parent.width - 40

        RowLayout {
            anchors.fill: parent
            spacing: 12

            // Category panel: the browse-tree panel from Library/Browser.qml.
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 280
                border.color: Theme.panelBorderColor
                border.width: 1
                color: Theme.sunkenBackgroundColor
                radius: 4

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 1
                    spacing: 5

                    Rectangle {
                        Layout.fillWidth: true
                        color: Theme.toolbarBackgroundColor
                        implicitHeight: 28
                        radius: 4

                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.deckTextColor
                            font.bold: true
                            font.pixelSize: 11
                            text: qsTr("SETTINGS")
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
                    Rectangle {
                        id: searchSetting

                        property bool active: false
                        property alias input: searchInput

                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        Layout.preferredHeight: 28
                        Layout.rightMargin: 6
                        border.color: active ? Theme.accentColor : Theme.panelBorderColor
                        border.width: 1
                        color: Theme.backgroundColor
                        radius: 4

                        Text {
                            id: searchInputPlaceholder

                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.midGray
                            font.pixelSize: 12
                            text: qsTr("Search...")
                            visible: !parent.active
                        }
                        TextInput {
                            id: searchInput

                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            clip: true
                            color: Theme.white
                            font.pixelSize: 12
                            visible: parent.active

                            onActiveFocusChanged: {
                                parent.active = activeFocus;
                            }
                            onTextEdited: {
                                root.manager.search(text);
                            }
                        }
                        TapHandler {
                            onTapped: {
                                parent.active = true;
                                searchInput.forceActiveFocus();
                            }
                        }
                    }
                    ListView {
                        id: categoryList

                        Layout.bottomMargin: 6
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                        clip: true
                        currentIndex: 0
                        focus: true
                        model: sectionProperties
                        visible: !searchSetting.active

                        delegate: Rectangle {
                            id: categoryRow

                            readonly property bool current: ListView.isCurrentItem
                            required property int index
                            required property var label

                            color: current ? Qt.alpha(Theme.blue, 0.16) : (rowHover.hovered ? Qt.alpha(Theme.white, 0.05) : "transparent")
                            height: 38
                            radius: 4
                            width: ListView.view.width

                            HoverHandler {
                                id: rowHover
                            }
                            Image {
                                id: handleImage

                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                fillMode: Image.PreserveAspectFit
                                height: 20
                                source: "images/gear.svg"
                                visible: false
                            }
                            ColorOverlay {
                                anchors.fill: handleImage
                                antialiasing: true
                                color: categoryRow.current ? Theme.blue : Theme.textColor
                                // Sidebar icons sit back at 0.7 and lift on hover, full on the
                                // current row -- same as the browse tree.
                                opacity: categoryRow.current ? 1 : (rowHover.hovered ? 0.85 : 0.7)
                                source: handleImage
                            }
                            Text {
                                anchors.left: handleImage.right
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.white
                                elide: Text.ElideRight
                                font.bold: categoryRow.current
                                font.pixelSize: 12
                                text: label
                            }
                            TapHandler {
                                onTapped: {
                                    categoryList.currentIndex = index;
                                }
                            }
                        }
                    }
                    ListView {
                        id: settingResultList

                        Layout.bottomMargin: 6
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                        clip: true
                        focus: true
                        model: root.manager.model
                        visible: searchSetting.active

                        delegate: Rectangle {
                            required property var display
                            required property int index
                            required property var toolTip
                            required property var whatsThis

                            color: resultHover.hovered ? Qt.alpha(Theme.white, 0.05) : "transparent"
                            height: 40
                            radius: 4
                            width: ListView.view.width

                            HoverHandler {
                                id: resultHover
                            }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.margins: 4
                                anchors.rightMargin: 8

                                Text {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: implicitHeight
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 12
                                    text: searchSetting.input.text ? display.replace(searchSetting.input.text, `<b>${searchSetting.input.text}</b>`) : display
                                    textFormat: Text.RichText
                                }
                                Text {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: implicitHeight
                                    color: Theme.midGray
                                    elide: Text.ElideRight
                                    font.pixelSize: 10
                                    text: searchSetting.input.text ? whatsThis.replace(searchSetting.input.text, `<b>${searchSetting.input.text}</b>`) : whatsThis
                                    textFormat: Text.RichText
                                }
                            }
                            TapHandler {
                                onTapped: {
                                    for (let setting of toolTip) {
                                        setting.activated();
                                    }
                                    parent.forceActiveFocus();
                                }
                            }
                        }
                    }
                }
            }
            // Page pane: header strip naming the active category, then its tabs,
            // then the page itself on the main-window ground so the pages' own
            // sunken boxes keep their contrast.
            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.toolbarBackgroundColor
                    implicitHeight: 28
                    radius: 4

                    Label {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: closeButton.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.deckTextColor
                        elide: Text.ElideRight
                        font.bold: true
                        font.capitalization: Font.AllUppercase
                        font.pixelSize: 11
                        text: root.activeCategory?.label ?? qsTr("Settings")
                    }
                    Skin.Button {
                        id: closeButton

                        activeColor: Theme.white
                        anchors.right: parent.right
                        anchors.rightMargin: 3
                        anchors.verticalCenter: parent.verticalCenter
                        fontPixelSize: 12
                        implicitHeight: 22
                        implicitWidth: 26
                        text: "✕"

                        onClicked: root.close()
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
                Item {
                    id: tabBar

                    readonly property int selectedIndex: root.activeCategory?.selectedIndex ?? 0
                    readonly property var tabs: root.activeCategory?.tabs ?? []

                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    visible: tabs?.length > 0

                    RowLayout {
                        anchors.fill: parent

                        Repeater {
                            model: tabBar.tabs

                            Skin.Button {
                                required property int index
                                required property string modelData

                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 22
                                Layout.preferredWidth: parent.width / (tabBar.tabs.length + 2)
                                activeColor: Theme.white
                                checked: tabBar.selectedIndex == index
                                text: modelData

                                onPressed: {
                                    if (root.activeCategory?.selectedIndex || root.activeCategory?.selectedIndex === 0) {
                                        root.activeCategory.selectedIndex = index;
                                    }
                                }
                            }
                        }
                    }
                }
                Mixxx.SettingParameterManager {
                    id: managerItem

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.leftMargin: 20

                    Component.onCompleted: {
                        let activateBuilder = index => function () {
                                categoryList.currentIndex = index;
                            };
                        let visibleBuilder = index => function () {
                                return categoryList.currentIndex == index;
                            };
                        for (let index = 0; index < data.length; index++) {
                            let child = data[index];
                            if (!child.label)
                                continue;
                            sectionProperties.append({
                                label: child.label
                            });
                            child.visible = Qt.binding(visibleBuilder(index));
                            child.activated.connect(activateBuilder(index));
                            child.anchors.fill = this;
                        }
                        // This is needed to ensure the right category is displayed.
                        // It would seems there is a bug, where the component's layout appears out of date.
                        // Setting the value to its current one seems to be triggering a component update which help fixing the layout
                        root.activeCategoryIndex = root.activeCategoryIndex;
                    }

                    Settings.SoundHardware {
                    }
                    Settings.Library {
                    }
                    Settings.Controller {
                    }
                    Settings.Interface {
                    }
                    Settings.MixerEffect {
                    }
                    Settings.AutoDJ {
                    }
                    Settings.Broadcast {
                    }
                    Settings.Recording {
                    }
                    Settings.Analyzer {
                    }
                    Settings.StatsPerformance {
                    }
                }
            }
        }
    }

    onActiveCategoryIndexChanged: updateActiveCategory()
    onSectionsChanged: updateActiveCategory()

    ListModel {
        id: sectionProperties
    }
}
