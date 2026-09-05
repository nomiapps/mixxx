import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import "Theme"
import "Settings" as Settings

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

    horizontalPadding: 12
    verticalPadding: 12
    // Stated rather than inherited: this popup had no close button and no declared
    // policy, so the only way out was Escape -- and nothing on screen said so. When it
    // fills the window there is no "outside" left to click either.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // CloseOnEscape only fires while the popup holds ACTIVE FOCUS, so the policy alone
    // did nothing: Escape closed nested dialogs, which take focus themselves, but never
    // this window.
    focus: true

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
        height: parent.height - 24
        width: parent.width - 24

        RowLayout {
            anchors.fill: parent
            spacing: 8

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 260
                border.color: Theme.panelBorderColor
                border.width: 1
                color: Theme.sunkenBackgroundColor
                radius: 4

                ColumnLayout {
                    anchors.fill: parent
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
                            text: qsTr("PREFERENCES")
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
                        id: searchSetting

                        property bool active: false
                        property alias input: searchInput

                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        height: 28

                        TextField {
                            id: searchInput

                            anchors.fill: parent
                            color: Theme.deckTextColor
                            font.pixelSize: 13
                            placeholderText: qsTranslate("WSearchLineEdit", "Search...")
                            placeholderTextColor: Theme.midGray

                            background: Rectangle {
                                border.color: searchInput.activeFocus ? Theme.blue : Theme.midGray
                                border.width: 1
                                color: Theme.knobBackgroundColor
                                radius: 4
                            }

                            onActiveFocusChanged: {
                                searchSetting.active = activeFocus;
                            }
                            onTextEdited: {
                                root.manager.search(text);
                            }
                        }
                    }
                    ListView {
                        id: categoryList

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.bottomMargin: 6
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                        clip: true
                        currentIndex: 0
                        focus: true
                        model: sectionProperties
                        spacing: 1
                        visible: !searchSetting.active

                        delegate: Rectangle {
                            id: categoryRow

                            required property int index
                            required property var label

                            property bool hovered: categoryHover.containsMouse

                            color: ListView.isCurrentItem ? Qt.rgba(0.004, 0.863, 0.988, 0.16) : (hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                            height: 38
                            radius: 4
                            width: ListView.view.width

                            MouseArea {
                                id: categoryHover

                                anchors.fill: parent
                                hoverEnabled: true

                                onClicked: categoryList.currentIndex = categoryRow.index
                            }
                            Image {
                                id: handleImage

                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                fillMode: Image.PreserveAspectFit
                                height: 16
                                source: "images/gear.svg"
                                sourceSize.height: 32
                                sourceSize.width: 32
                                visible: false
                                width: 16
                            }
                            ColorOverlay {
                                anchors.fill: handleImage
                                antialiasing: true
                                color: categoryRow.ListView.isCurrentItem ? Theme.white : Theme.deckTextColor
                                opacity: categoryRow.ListView.isCurrentItem ? 1 : (categoryRow.hovered ? 0.9 : 0.55)
                                source: handleImage
                            }
                            Label {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: handleImage.left
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                color: categoryRow.ListView.isCurrentItem ? Theme.white : Theme.deckTextColor
                                elide: Text.ElideRight
                                font.pixelSize: 14
                                font.weight: categoryRow.ListView.isCurrentItem ? Font.Bold : Font.Medium
                                text: categoryRow.label
                            }
                        }
                    }
                    ListView {
                        id: settingResultList

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.bottomMargin: 6
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                        clip: true
                        focus: true
                        model: root.manager.model
                        spacing: 1
                        visible: searchSetting.active

                        delegate: Rectangle {
                            id: resultRow

                            required property var display
                            required property int index
                            required property var toolTip
                            required property var whatsThis

                            property bool hovered: resultHover.containsMouse

                            color: hovered ? Qt.rgba(0.004, 0.863, 0.988, 0.16) : "transparent"
                            height: 44
                            radius: 4
                            width: ListView.view.width

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                anchors.margins: 4
                                spacing: 0

                                Text {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: implicitHeight
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 13
                                    text: searchSetting.input.text ? display.replace(searchSetting.input.text, `<b>${searchSetting.input.text}</b>`) : display
                                    textFormat: Text.RichText
                                }
                                Text {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: implicitHeight
                                    color: Theme.midGray
                                    elide: Text.ElideRight
                                    font.pixelSize: 11
                                    text: searchSetting.input.text ? whatsThis.replace(searchSetting.input.text, `<b>${searchSetting.input.text}</b>`) : whatsThis
                                    textFormat: Text.RichText
                                }
                            }
                            MouseArea {
                                id: resultHover

                                anchors.fill: parent
                                hoverEnabled: true

                                onClicked: {
                                    for (let setting of resultRow.toolTip) {
                                        setting.activated();
                                    }
                                    resultRow.forceActiveFocus();
                                }
                            }
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                border.color: Theme.panelBorderColor
                border.width: 1
                color: Theme.sunkenBackgroundColor
                radius: 4

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

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
                            font.pixelSize: 11
                            text: (root.activeCategory && root.activeCategory.label) ? root.activeCategory.label.toUpperCase() : qsTr("SETTINGS")
                        }
                        Skin.FormButton {
                            id: closeButton

                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            implicitHeight: 22
                            implicitWidth: 22
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
                    Rectangle {
                        id: tabBar

                        readonly property int selectedIndex: root.activeCategory?.selectedIndex ?? 0
                        readonly property var tabs: root.activeCategory?.tabs ?? []

                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        Layout.topMargin: 6
                        implicitHeight: 28
                        color: "transparent"
                        visible: tabs?.length > 0

                        Row {
                            anchors.fill: parent
                            spacing: 4

                            Repeater {
                                model: tabBar.tabs

                                Rectangle {
                                    id: tabChip

                                    required property int index
                                    required property string modelData

                                    property bool hovered: tabHover.containsMouse
                                    property bool selected: tabBar.selectedIndex == index

                                    color: selected ? Qt.rgba(0.004, 0.863, 0.988, 0.16) : (hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                                    height: parent.height
                                    radius: 4
                                    width: Math.max(72, tabLabel.implicitWidth + 20)

                                    Label {
                                        id: tabLabel

                                        anchors.centerIn: parent
                                        color: tabChip.selected ? Theme.white : Theme.deckTextColor
                                        font.bold: tabChip.selected
                                        font.pixelSize: 11
                                        text: tabChip.modelData.toUpperCase()
                                    }
                                    MouseArea {
                                        id: tabHover

                                        anchors.fill: parent
                                        hoverEnabled: true

                                        onClicked: {
                                            if (root.activeCategory?.selectedIndex || root.activeCategory?.selectedIndex === 0) {
                                                root.activeCategory.selectedIndex = tabChip.index;
                                            }
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
                        Layout.leftMargin: 16
                        Layout.rightMargin: 8
                        Layout.topMargin: 8

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
    }

    onActiveCategoryIndexChanged: updateActiveCategory()
    onSectionsChanged: updateActiveCategory()

    ListModel {
        id: sectionProperties

    }
}
