import Mixxx 1.0 as Mixxx
import Edge.Controls 1.0 as EdgeControls
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Shapes
import "../Theme"

Item {
    id: root

    required property string group
    readonly property real platterSize: Math.min(width, height)
    readonly property bool loaded: trackLoadedControl.value > 0
    readonly property bool scratching: scratchEnableControl.value > 0
    readonly property var currentTrack: deckPlayer?.currentTrack
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: playPositionControl

        group: root.group
        key: "playposition"
    }
    Mixxx.ControlProxy {
        id: scratchEnableControl

        group: root.group
        key: "scratch_position_enable"
    }
    Mixxx.ControlProxy {
        id: bpmControl

        group: root.group
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: rateRatioControl

        group: root.group
        key: "rate_ratio"
    }

    Rectangle {
        id: bezel

        anchors.centerIn: parent
        color: "#08090a"
        height: root.platterSize
        radius: width / 2
        width: root.platterSize

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            border.color: root.scratching ? Theme.blue : "#3b4045"
            border.width: root.scratching ? 3 : 1
            color: "#111315"
            radius: width / 2

            Behavior on border.color {
                ColorAnimation {
                    duration: 90
                }
            }
            Behavior on border.width {
                NumberAnimation {
                    duration: 90
                }
            }
        }

        Repeater {
            model: 7

            Rectangle {
                required property int index

                anchors.centerIn: parent
                border.color: index % 2 ? "#202326" : "#191c1f"
                border.width: 1
                color: "transparent"
                height: width
                radius: width / 2
                width: bezel.width * (0.9 - index * 0.07)
            }
        }

        EdgeControls.Spinny {
            id: spinnyIndicator

            anchors.fill: parent
            anchors.margins: 5
            group: root.group
            indicatorVisible: root.loaded

            indicator: Item {
                height: spinnyIndicator.height
                width: spinnyIndicator.width

                Rectangle {
                    anchors.centerIn: parent
                    border.color: "#050505"
                    border.width: 2
                    color: "#181a1c"
                    height: width
                    radius: width / 2
                    width: parent.width * 0.54
                }
                Image {
                    id: coverArt

                    anchors.centerIn: parent
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    height: width
                    source: root.currentTrack?.coverArtUrl ?? ""
                    sourceSize.height: height * Screen.devicePixelRatio
                    sourceSize.width: width * Screen.devicePixelRatio
                    visible: false
                    width: parent.width * 0.52
                }
                Rectangle {
                    id: coverArtMask

                    anchors.fill: coverArt
                    radius: width / 2
                    visible: false
                }
                OpacityMask {
                    anchors.fill: coverArt
                    maskSource: coverArtMask
                    source: coverArt
                    visible: coverArt.status === Image.Ready
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    color: root.scratching ? Theme.blue : Theme.white
                    height: parent.height * 0.25
                    radius: width / 2
                    width: Math.max(2, parent.width * 0.018)

                    Behavior on color {
                        ColorAnimation {
                            duration: 90
                        }
                    }
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: root.scratching ? Theme.white : Theme.blue
                    height: width
                    radius: width / 2
                    width: Math.max(5, parent.width * 0.045)
                }
                Rectangle {
                    anchors.centerIn: parent
                    color: "#090a0b"
                    height: width
                    radius: width / 2
                    width: Math.max(6, parent.width * 0.055)

                    Rectangle {
                        anchors.centerIn: parent
                        color: Theme.lightGray2
                        height: width
                        radius: width / 2
                        width: parent.width * 0.35
                    }
                }
            }
        }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            visible: root.loaded

            ShapePath {
                fillColor: "transparent"
                strokeColor: Theme.blue
                strokeWidth: root.scratching ? 3 : 2

                PathAngleArc {
                    centerX: bezel.width / 2
                    centerY: bezel.height / 2
                    radiusX: bezel.width / 2 - 4
                    radiusY: radiusX
                    startAngle: -90
                    sweepAngle: Math.max(0, Math.min(1, playPositionControl.value)) * 360
                }
            }
        }

        Rectangle {
            id: tempoBadge

            anchors.centerIn: parent
            border.color: root.scratching ? Theme.blue : "#424951"
            border.width: 2
            color: "#e60b0d0f"
            height: Math.max(48, parent.height * 0.38)
            radius: width / 2
            visible: root.loaded
            width: height

            Column {
                anchors.centerIn: parent
                spacing: 0

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.white
                    font.bold: true
                    font.pixelSize: 12
                    text: bpmControl.value > 0 ? bpmControl.value.toFixed(1) : "--.-"
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.midGray
                    font.pixelSize: 7
                    text: "BPM"
                }
                Label {
                    readonly property real percent: (rateRatioControl.value - 1) * 100

                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.blue
                    font.pixelSize: 9
                    text: (percent >= 0 ? "+" : "") + percent.toFixed(1) + "%"
                }
            }

            Behavior on border.color {
                ColorAnimation {
                    duration: 90
                }
            }
        }
    }
}
