import QtQuick 2.12
import QtQuick.Shapes
import ".." as Skin
import "../Theme"

Skin.ControlButton {
    id: root

    property bool minimized: false

    activeColor: Theme.deckActiveColor
    compact: root.minimized
    key: "play"
    text: root.highlight ? qsTr("Pause") : qsTr("Play")
    toggleable: true
    glyph: Item {
        implicitHeight: 20
        implicitWidth: 20

        Shape {
            anchors.fill: parent
            antialiasing: true
            layer.enabled: true
            layer.samples: 4
            visible: !root.highlight

            ShapePath {
                capStyle: ShapePath.RoundCap
                fillColor: root.faceColor
                fillRule: ShapePath.WindingFill
                startX: 4
                startY: 1
                strokeColor: "transparent"

                PathLine {
                    x: 18
                    y: 10
                }
                PathLine {
                    x: 4
                    y: 19
                }
                PathLine {
                    x: 4
                    y: 1
                }
            }
        }
        Row {
            anchors.centerIn: parent
            spacing: 4
            visible: root.highlight

            Rectangle {
                color: root.faceColor
                height: 16
                radius: 1
                width: 5
            }
            Rectangle {
                color: root.faceColor
                height: 16
                radius: 1
                width: 5
            }
        }
    }
}
