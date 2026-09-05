import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import "Theme"

AbstractButton {
    id: root

    property color activeColor: Theme.buttonActiveColor
    property bool compact: false
    readonly property color faceColor: root.pressed ? root.pressedColor : ((root.highlight || root.checked) ? root.activeColor : root.normalColor)
    property alias fontPixelSize: label.font.pixelSize
    property Component glyph: null
    property bool highlight: false
    property color normalColor: Theme.buttonNormalColor
    property color pressedColor: activeColor

    implicitHeight: 26
    implicitWidth: 52

    background: Rectangle {
        id: backgroundImage

        // Drawn as vector instead of a 9-slice SVG. button.svg is a 58x26 raster
        // source: it was stretched to the button and then upscaled again on a
        // fractional-DPR screen (1.5x here), which stair-stepped the rounded
        // corners. BorderImage cannot be told to rasterise larger -- sourceSize
        // is read-only there -- but the artwork is only a rounded rect with a
        // half-opaque surround and a vertical gradient, so QML draws it exactly,
        // crisp at any scale and cheaper than sampling an image.
        // Colours taken from button.svg / button_pressed.svg; the states below
        // shift the gradient instead of swapping the source.
        property color bottomColor: "#202020"
        property color topColor: "#282828"

        color: Qt.rgba(0, 0, 0, 0.502)
        radius: 3

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 1

            gradient: Gradient {
                GradientStop {
                    color: backgroundImage.topColor
                    position: 0
                }
                GradientStop {
                    color: backgroundImage.bottomColor
                    position: 1
                }
            }
        }
    }
    contentItem: Item {
        anchors.fill: parent

        Glow {
            id: labelGlow

            anchors.fill: parent
            color: label.color
            radius: 4
            source: face
            spread: 0.1
        }
        Item {
            id: face

            anchors.fill: parent

            Column {
                anchors.centerIn: parent
                spacing: 4
                width: parent.width

                Loader {
                    id: glyphLoader

                    anchors.horizontalCenter: parent.horizontalCenter
                    height: visible ? 20 : 0
                    sourceComponent: root.glyph
                    visible: root.glyph !== null && !root.compact
                    width: 20
                }
                Label {
                    id: label

                    anchors.horizontalCenter: parent.horizontalCenter
                    color: root.normalColor
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.buttonFontPixelSize
                    horizontalAlignment: Text.AlignHCenter
                    text: root.text
                    verticalAlignment: Text.AlignVCenter
                    visible: root.text
                    width: Math.max(8, parent.width - 12)
                }
            }
        }
        Image {
            id: image

            anchors.centerIn: parent
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            height: icon.height
            source: icon.source
            // SVGs rasterise at sourceSize, so an icon left unset is drawn at its
            // natural size and rescaled -- visibly jagged. Rasterise at the size it
            // is actually drawn, times the screen's DPR.
            sourceSize.height: height * Screen.devicePixelRatio
            sourceSize.width: width * Screen.devicePixelRatio
            visible: false
            width: icon.width
        }
        ColorOverlay {
            anchors.fill: image
            antialiasing: true
            color: root.normalColor
            source: image
            visible: icon.source != null
        }
    }
    states: [
        State {
            name: "pressed"
            when: root.pressed

            PropertyChanges {
                bottomColor: "#181818"
                target: backgroundImage
                topColor: "#202020"
            }
            PropertyChanges {
                color: root.pressedColor
                target: label
            }
            PropertyChanges {
                target: labelGlow
                visible: true
            }
        },
        State {
            name: "active"
            when: (root.highlight || root.checked) && !root.pressed

            PropertyChanges {
                bottomColor: "#202020"
                target: backgroundImage
                topColor: "#282828"
            }
            PropertyChanges {
                color: root.activeColor
                target: label
            }
            PropertyChanges {
                target: labelGlow
                visible: true
            }
        },
        State {
            name: "inactive"
            when: !root.checked && !root.highlight && !root.pressed

            PropertyChanges {
                bottomColor: "#202020"
                target: backgroundImage
                topColor: "#282828"
            }
            PropertyChanges {
                color: root.normalColor
                target: label
            }
            PropertyChanges {
                target: labelGlow
                visible: false
            }
        }
    ]
}
