import Qt5Compat.GraphicalEffects
import QtQuick 2.12
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

    background: BorderImage {
        id: backgroundImage

        anchors.fill: parent
        horizontalTileMode: BorderImage.Stretch
        source: Theme.imgButton
        verticalTileMode: BorderImage.Stretch

        border {
            bottom: 10
            left: 10
            right: 10
            top: 10
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
                source: Theme.imgButtonPressed
                target: backgroundImage
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
                source: Theme.imgButton
                target: backgroundImage
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
                source: Theme.imgButton
                target: backgroundImage
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
