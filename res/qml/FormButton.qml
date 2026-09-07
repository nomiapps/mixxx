import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import "Theme"

// The form button of the New UI chrome: a flat 4px-radius face behind a 1px
// hairline, the way the settings shell draws its fields and rows. Used by the
// settings pages and the library toolbar. It used to be a stack of two inner
// shadows, a drop shadow and a glow around a solid slab, and the slab went
// solid cyan for the primary action; none of that exists elsewhere in the chrome.
AbstractButton {
    id: root

    readonly property bool active: root.highlight || root.checked
    property color activeColor: Theme.deckActiveColor
    // Face colour. Callers pass Theme.warningColor for destructive actions.
    // The primary action is a flag, below, not a colour.
    property color backgroundColor: Theme.controlFaceColor
    property bool highlight: false
    property color normalColor: Theme.white
    property color pressedColor: activeColor
    // The one thing the form wants you to press, drawn the way the chrome draws
    // the current row and a focused field: Theme.blue tint and hairline.
    property bool primary: false
    readonly property bool tinted: root.primary || root.active

    implicitHeight: 26
    implicitWidth: 98

    background: Rectangle {
        border.color: root.tinted ? Theme.blue : Theme.panelBorderColor
        border.width: 1
        color: root.pressed ? Theme.pressedWashColor : (root.tinted ? Theme.selectionColor : root.backgroundColor)
        radius: 4

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: Theme.hoverWashColor
            radius: 3
            visible: root.hovered && !root.pressed
        }
    }
    contentItem: Item {
        anchors.fill: parent

        Label {
            id: label

            anchors.fill: parent
            color: root.pressed ? root.pressedColor : (root.active ? root.activeColor : root.normalColor)
            elide: Text.ElideRight
            font.bold: true
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.pixelSize: Theme.buttonFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            leftPadding: 6
            rightPadding: 6
            text: root.text
            verticalAlignment: Text.AlignVCenter
            visible: root.text != null
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
            color: label.color
            source: image
            visible: icon.source != null
        }
    }
}
