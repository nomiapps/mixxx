import Mixxx 1.0 as Mixxx
import QtQuick.Shapes
import QtQuick 2.12
import ".." as Skin
import "../Theme"

Item {
    id: root

    property color buttonColor: trackLoadedControl.value > 0 ? Theme.buttonActiveColor : Theme.buttonDisableColor
    required property string group

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Rectangle {
        anchors.fill: parent
        border.color: "#30343d"
        border.width: 1
        color: "#111216"
        radius: 5
    }
    Skin.ControlButton {
        id: reverseButton

        activeColor: Theme.deckActiveColor
        group: root.group
        implicitHeight: 22
        implicitWidth: 22
        key: "reverse"

        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
        }

        background: Rectangle {
            border.color: reverseButton.highlight ? Theme.accentColor : "#343740"
            border.width: 1
            color: reverseButton.highlight ? "#203b78" : (reverseButton.pressed ? "#252b36" : "#17181b")
            radius: 4

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.accentColor
                height: 2
                visible: reverseButton.highlight
                width: parent.width - 8
            }
        }

        contentItem: Shape {
            // Qt 6.6+ resolution-independent antialiasing; the older
            // geometry renderer stair-steps curves on some displays.
            preferredRendererType: Shape.CurveRenderer
            anchors.fill: parent
            antialiasing: true

            ShapePath {
                fillColor: reverseButton.highlight ? Theme.white : root.buttonColor
                startX: 5
                startY: 11
                strokeColor: 'transparent'

                PathLine {
                    x: 20
                    y: 4
                }
                PathLine {
                    x: 20
                    y: 18
                }
                PathLine {
                    x: 5
                    y: 11
                }
            }
        }
    }
    Skin.Button {
        id: beatgridButton

        anchors.right: ejectButton.left
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        implicitHeight: 22
        normalColor: trackLoadedControl.value > 0 ? Theme.lightGray2 : Theme.buttonDisableColor
        text: "Beatgrid"
        visible: root.width > 165

        background: Rectangle {
            border.color: beatgridButton.pressed ? Theme.accentColor : "#343740"
            border.width: 1
            color: beatgridButton.pressed ? "#252b36" : "#17181b"
            radius: 4
        }
    }
    Skin.ControlButton {
        id: ejectButton

        activeColor: Theme.deckActiveColor
        anchors.right: parent.right
        group: root.group
        implicitHeight: 22
        implicitWidth: 22
        key: "eject"

        anchors.verticalCenter: parent.verticalCenter

        background: Rectangle {
            border.color: ejectButton.pressed ? Theme.accentColor : "#343740"
            border.width: 1
            color: ejectButton.pressed ? "#252b36" : "#17181b"
            radius: 4
        }

        contentItem: Item {
            anchors.fill: parent

            Shape {
                // Qt 6.6+ resolution-independent antialiasing; the older
                // geometry renderer stair-steps curves on some displays.
                preferredRendererType: Shape.CurveRenderer
                antialiasing: true
                height: 10
                width: 15

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    top: parent.top
                    topMargin: 5
                }
                ShapePath {
                    fillColor: root.buttonColor
                    startX: 7.5
                    startY: 0
                    strokeColor: 'transparent'

                    PathLine {
                        x: 15
                        y: 10
                    }
                    PathLine {
                        x: 0
                        y: 10
                    }
                    PathLine {
                        x: 7.5
                        y: 0
                    }
                }
            }
            Rectangle {
                color: root.buttonColor
                height: 2
                width: 15

                anchors {
                    bottom: parent.bottom
                    bottomMargin: 3
                    horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }
}
