import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Controls 2.12
import "../Theme"

Item {
    id: root

    required property var currentTrack
    required property string group

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }
    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Rectangle {
        id: surface

        border.color: "#30343d"
        border.width: 1
        color: "#111216"
        radius: 6

        states: [
            State {
                name: "inactive"
                when: trackLoadedControl.value == 0

                PropertyChanges {
                    checked: false
                    opacity: 0.45
                    target: hotcueTabButton
                }
                PropertyChanges {
                    checked: false
                    opacity: 0.45
                    target: stemTabButton
                }
                PropertyChanges {
                    target: stemTab
                    visible: false
                }
                PropertyChanges {
                    target: hotcueTab
                    visible: false
                }
            },
            State {
                name: "hotcue"
                when: hotcueTabButton.checked || stemCountControl.value == 0

                PropertyChanges {
                    target: hotcueTab
                    visible: true
                }
                PropertyChanges {
                    target: stemTab
                    visible: false
                }
            },
            State {
                name: "stem"
                when: stemTabButton.checked && stemCountControl.value != 0

                PropertyChanges {
                    target: stemTab
                    visible: true
                }
                PropertyChanges {
                    target: hotcueTab
                    visible: false
                }
            }
        ]

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: stemCountControl.value > 0 ? tabs.left : parent.right
            rightMargin: stemCountControl.value > 0 ? 6 : 0
            top: parent.top
        }
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.top: parent.top
            color: Theme.accentColor
            height: 1
            opacity: 0.55
        }
        Item {
            id: hotcueTab

            anchors.fill: parent
            visible: false

            Skin.FadeBehavior on visible {
                fadeTarget: hotcueTab
            }

            GridLayout {
                anchors.bottomMargin: 7
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                anchors.topMargin: 7
                columnSpacing: 6
                rowSpacing: 6

                Repeater {
                    model: 8

                    Item {
                        required property int index

                        Layout.column: index % 4
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.row: parseInt(index / 4)

                        Skin.Hotcue {
                            id: hotcue

                            readonly property var label: isSet ? root.currentTrack.hotcuesModel.get(index).label : null

                            activate: activator.pressedButtons == Qt.LeftButton
                            // onIsSetChanged: {
                            //     if (!isSet)
                            //         popup.close();
                            // }
                            group: root.group
                            hotcueNumber: index + 1
                        }
                        Rectangle {
                            id: backgroundImage

                            anchors.fill: parent
                            border.color: hotcue.isSet ? Qt.lighter(hotcue.color, 1.15) : (activator.containsMouse ? Theme.accentColor : "#383b44")
                            border.width: 1
                            color: hotcue.isSet ? Qt.darker(hotcue.color, 1.25) : "#202124"
                            radius: 4

                            MouseArea {
                                id: activator

                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                anchors.fill: parent
                                hoverEnabled: true

                                Rectangle {
                                    anchors.fill: parent
                                    color: "#99000000"
                                    radius: backgroundImage.radius
                                    visible: activator.pressed
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: hotcue.color
                                height: 2
                                radius: 1
                                visible: hotcue.isSet
                                width: parent.width - 10
                            }
                        }
                        ColumnLayout {
                            anchors.centerIn: backgroundImage
                            spacing: 0

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                color: hotcue.isSet ? Theme.white : Theme.lightGray3
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                text: `${index + 1}`
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                color: hotcue.isSet ? Theme.white : Theme.lightGray3
                                font.pixelSize: 10
                                text: hotcue.label ?? ""
                                visible: hotcue.label
                            }
                        }
                    }
                }
            }
        }
        Item {
            id: stemTab

            anchors.fill: parent
            visible: false

            Skin.FadeBehavior on visible {
                fadeTarget: stemTab
            }

            RowLayout {
                anchors.bottomMargin: 7
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.topMargin: 7
                spacing: 6

                Repeater {
                    model: root.currentTrack.stemsModel

                    Item {
                        id: stem

                        required property color color
                        readonly property string fxGroup: `[QuickEffectRack1_${group}]`
                        readonly property string group: `${root.group.substr(0, root.group.length - 1)}_Stem${index + 1}]`
                        required property int index
                        required property string label

                        Layout.fillHeight: true
                        Layout.fillWidth: true

                        Rectangle {
                            anchors.fill: parent
                            border.color: Qt.darker(stem.color, 2.0)
                            border.width: 1
                            color: "#191a1e"
                            radius: 4
                        }
                        Item {
                            id: stemButton

                            height: parent.height / 3 * 2
                            width: parent.width / 3 * 2

                            anchors {
                                bottom: stemFxSelector.top
                                left: parent.left
                                top: parent.top
                            }
                            Rectangle {
                                anchors.fill: parent
                                border.color: stemMute.value ? "#3a3d46" : Qt.lighter(stem.color, 1.12)
                                border.width: 1
                                color: stemMute.value ? Qt.darker(stem.color, 2.4) : Qt.darker(stem.color, 1.35)
                                radius: 4
                            }
                            Item {
                                anchors.fill: parent
                                anchors.margins: 6
                                clip: true

                                Label {
                                    id: stemLabel

                                    readonly property bool rotated: fontMetrics.advanceWidth > parent.width && parent.height >= parent.width

                                    anchors.centerIn: parent
                                    color: stemMute.value ? Theme.lightGray3 : Theme.white
                                    elide: Text.ElideRight
                                    font.weight: Font.Bold
                                    height: font.pixelSize
                                    horizontalAlignment: rotated ? Text.AlignLeft : Text.AlignHCenter
                                    text: stem.label
                                    width: rotated ? parent.height : parent.width

                                    transform: Rotation {
                                        angle: stemLabel.rotated ? 90 : 0
                                        origin.x: stemLabel.width / 2
                                        origin.y: stemLabel.height / 2
                                    }

                                    TextMetrics {
                                        id: fontMetrics

                                        font: stemLabel.font
                                        text: stem.label
                                    }
                                }
                            }
                            Mixxx.ControlProxy {
                                id: stemMute

                                group: stem.group
                                key: "mute"
                            }
                            TapHandler {
                                onTapped: stemMute.value = !stemMute.value
                            }
                        }
                        Skin.ComboBox {
                            id: stemFxSelector

                            clip: true
                            currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
                            font.pixelSize: 10
                            height: Math.min(parent.width, parent.height) / 3
                            indicator.width: 0
                            model: Mixxx.EffectsManager.quickChainPresetModel
                            popupMaxItem: 8
                            popupWidth: 100
                            spacing: 2
                            textRole: "display"
                            width: parent.width / 3 * 2

                            onActivated: index => {
                                fxSelect.value = index;
                            }

                            anchors {
                                bottom: parent.bottom
                                left: parent.left
                            }
                            Mixxx.ControlProxy {
                                id: fxSelect

                                group: stem.fxGroup
                                key: "loaded_chain_preset"
                            }
                        }
                        Item {
                            id: stemVolume

                            width: parent.width / 3

                            anchors {
                                bottom: stemFxKnob.top
                                bottomMargin: 3
                                left: stemButton.right
                                right: parent.right
                                top: parent.top
                            }

                            // Skin.VuMeter {
                            //     x: 15
                            //     y: (parent.height - height) / 2
                            //     width: 4
                            //     height: parent.height - 40
                            //     group: root.group
                            //     key: "vu_meter_left"
                            // }

                            // Skin.VuMeter {
                            //     x: parent.width - width - 15
                            //     y: (parent.height - height) / 2
                            //     width: 4
                            //     height: parent.height - 40
                            //     group: root.group
                            //     key: "vu_meter_right"
                            // }

                            Skin.ControlFader {
                                id: volumeSlider

                                anchors.fill: parent
                                bar.color: Theme.volumeSliderBarColor
                                bg: Theme.imgVolumeSliderBackground
                                group: stem.group
                                implicitWidth: 10
                                key: "volume"

                                handleImage {
                                    width: parent.width - 4
                                }
                            }
                        }
                        Item {
                            id: stemFxKnob

                            height: stemFxSelector.height
                            width: parent.width / 3

                            anchors {
                                bottom: parent.bottom
                                right: parent.right
                            }
                            Skin.QuickFxKnob {
                                anchors.centerIn: parent
                                group: stem.fxGroup
                                height: width
                                knob.arcStyle: ShapePath.DashLine
                                knob.arcStylePattern: [2, 2]
                                knob.color: Theme.eqFxColor
                                showPreset: false
                                width: Math.min(parent.width, parent.height)

                                knob {
                                    height: width * 0.8
                                    width: width * 0.8
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    ColumnLayout {
        id: tabs

        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.top: parent.top
        spacing: 6
        width: stemCountControl.value > 0 ? 38 : 0

        Behavior on width {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }

        Skin.Button {
            id: hotcueTabButton

            Layout.fillWidth: true
            Layout.margins: 0
            activeColor: Theme.deckActiveColor
            checked: true
            implicitHeight: 30

            background: Rectangle {
                border.color: hotcueTabButton.checked ? Theme.accentColor : "#343740"
                border.width: 1
                color: hotcueTabButton.checked ? "#203b78" : (hotcueTabButton.pressed ? "#252b36" : "#17181b")
                radius: 4
            }

            contentItem: Shape {
                anchors.fill: parent
                antialiasing: true
                layer.enabled: true
                layer.samples: 4

                ShapePath {
                    fillColor: hotcueTabButton.checked ? Theme.white : Theme.lightGray3
                    startX: 10
                    startY: 4

                    PathLine {
                        x: 26
                        y: 4
                    }
                    PathLine {
                        x: 19.5
                        y: 10
                    }
                    PathLine {
                        x: 19.5
                        y: 24
                    }
                    PathLine {
                        x: 16.5
                        y: 24
                    }
                    PathLine {
                        x: 16.5
                        y: 10
                    }
                    PathLine {
                        x: 10
                        y: 4
                    }
                }
            }

            onClicked: {
                stemTabButton.checked = false;
                hotcueTabButton.checked = trackLoadedControl.value == 1;
            }
        }
        Skin.Button {
            id: stemTabButton

            Layout.fillWidth: true
            Layout.margins: 0
            activeColor: Theme.deckActiveColor
            implicitHeight: 30

            background: Rectangle {
                border.color: stemTabButton.checked ? Theme.accentColor : "#343740"
                border.width: 1
                color: stemTabButton.checked ? "#203b78" : (stemTabButton.pressed ? "#252b36" : "#17181b")
                radius: 4
            }

            contentItem: Item {
                Rectangle {
                    color: stemTabButton.checked ? Theme.white : Theme.lightGray3
                    height: 1

                    anchors {
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 5
                        top: parent.top
                        topMargin: 6
                    }
                }
                Rectangle {
                    color: stemTabButton.checked ? Theme.white : Theme.lightGray3
                    height: 1

                    anchors {
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 14
                        top: parent.top
                        topMargin: 12
                    }
                }
                Rectangle {
                    color: stemTabButton.checked ? Theme.white : Theme.lightGray3
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 12
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 22
                    }
                }
                Rectangle {
                    color: stemTabButton.checked ? Theme.white : Theme.lightGray3
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 13
                        left: parent.left
                        leftMargin: 22
                        right: parent.right
                        rightMargin: 5
                    }
                }
                Rectangle {
                    color: stemTabButton.checked ? Theme.white : Theme.lightGray3
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 6
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 26
                    }
                }
                Rectangle {
                    color: stemTabButton.checked ? Theme.white : Theme.lightGray3
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 6
                        left: parent.left
                        leftMargin: 14
                        right: parent.right
                        rightMargin: 15
                    }
                }
                Rectangle {
                    color: stemTabButton.checked ? Theme.white : Theme.lightGray3
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 6
                        left: parent.left
                        leftMargin: 26
                        right: parent.right
                        rightMargin: 5
                    }
                }
            }

            onClicked: {
                stemTabButton.checked = trackLoadedControl.value == 1 && stemCountControl.value != 0;
                hotcueTabButton.checked = !stemTabButton.checked;
            }
        }
    }
}
