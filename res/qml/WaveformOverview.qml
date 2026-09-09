import "." as Skin
import Mixxx 1.0 as Mixxx
import Edge.Controls 1.0 as EdgeControls
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group
    // The slice of the track to draw, as fractions of its length; the whole
    // track by default. Passed straight through to the overview and its markers.
    property real rangeStart: 0
    property real rangeEnd: 1

    states: [
        State {
            when: passthroughControl.value == 0

            PropertyChanges {
                target: waveformContainer
                opacity: 1
                visible: true
            }

            PropertyChanges {
                target: passthroughContainer
                opacity: 0
                visible: false
            }

        },
        State {
            when: passthroughControl.value != 0

            PropertyChanges {
                target: waveformContainer
                opacity: 0
                visible: false
            }

            PropertyChanges {
                target: passthroughContainer
                opacity: 1
                visible: true
            }
        }
    ]
    transitions: [
        Transition {
            enabled: passthroughContainer.visible

            SequentialAnimation {
                PropertyAction {
                    target: waveformContainer
                    property: "visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: waveformContainer
                        property: "opacity"
                        duration: 150
                    }

                    NumberAnimation {
                        target: passthroughContainer
                        property: "opacity"
                        duration: 150
                    }
                }

                PropertyAction {
                    target: passthroughContainer
                    property: "visible"
                }
            }

        },
        Transition {
            enabled: waveformContainer.visible

            SequentialAnimation {
                PropertyAction {
                    target: passthroughContainer
                    property: "visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: waveformContainer
                        property: "opacity"
                        duration: 150
                    }

                    NumberAnimation {
                        target: passthroughContainer
                        property: "opacity"
                        duration: 150
                    }
                }

                PropertyAction {
                    target: waveformContainer
                    property: "visible"
                }
            }
        }
    ]

    Mixxx.ControlProxy {
        id: passthroughControl

        group: root.group
        key: "passthrough"
    }

    Rectangle {
        id: waveformContainer

        anchors.fill: parent
        color: Theme.deckBackgroundColor

        EdgeControls.WaveformOverview {
            anchors.fill: parent
            channels: Mixxx.WaveformOverview.Channels.LeftChannel
            renderer: Mixxx.WaveformOverview.Renderer.Filtered
            colorHigh: Theme.white
            colorMid: Theme.blue
            colorLow: Theme.green
            group: root.group
            rangeEnd: root.rangeEnd
            rangeStart: root.rangeStart
        }
    }

    Rectangle {
        id: passthroughContainer

        anchors.fill: parent
        color: "transparent"

        Skin.SectionText {
            anchors.centerIn: parent
            text: "Passthrough Enabled"
        }
    }
}
