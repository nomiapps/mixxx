import Mixxx 1.0 as Mixxx
import QtQuick.Shapes
import QtQuick 2.12
import ".." as Skin
import "../Theme"

Item {
    id: root

    property color buttonColor: trackLoadedControl.value > 0 ? Theme.buttonActiveColor : Theme.buttonDisableColor
    // The beatgrid can only be moved when there is a grid to move and it is
    // not locked. An unanalysed track has no grid at all (file_bpm reads 0),
    // and locking a beatgrid is how you protect a grid you have already got
    // right. In both cases BpmControl drops the request without a word, so
    // the button greys out rather than silently refusing.
    readonly property bool canAdjustBeatgrid: trackLoadedControl.value > 0 && fileBpmControl.value > 0 && bpmLockControl.value === 0
    required property string group

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: fileBpmControl

        group: root.group
        key: "file_bpm"
    }
    Mixxx.ControlProxy {
        id: bpmLockControl

        group: root.group
        key: "bpmlock"
    }
    Mixxx.ControlProxy {
        id: beatsTranslateMatchControl

        group: root.group
        key: "beats_translate_match_alignment"
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
    // Moves the beatgrid so the nearest beat lands on the play position, which
    // is how you fix a grid whose beats are right but whose downbeat is off.
    // Right-click aligns it to the other deck instead. Same two actions, on the
    // same two buttons, as the legacy skins' beatgrid button.
    //
    // Upstream draws this button but never wired it to anything: it had no
    // click handler at all, so it looked live and did nothing.
    Skin.ControlButton {
        id: beatgridButton

        anchors.right: ejectButton.left
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        enabled: root.canAdjustBeatgrid
        group: root.group
        implicitHeight: 22
        key: "beats_translate_curpos"
        normalColor: root.canAdjustBeatgrid ? Theme.lightGray2 : Theme.buttonDisableColor
        text: "Beatgrid"
        visible: root.width > 165

        background: Rectangle {
            border.color: beatgridButton.pressed ? Theme.accentColor : "#343740"
            border.width: 1
            color: beatgridButton.pressed ? "#252b36" : "#17181b"
            radius: 4
        }

        // AbstractButton only takes the left button, so this does not fight the
        // primary action.
        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: root.canAdjustBeatgrid

            onTapped: {
                beatsTranslateMatchControl.value = 1;
                beatsTranslateMatchControl.value = 0;
            }
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
