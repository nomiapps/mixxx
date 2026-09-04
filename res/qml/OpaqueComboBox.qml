import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

// Compact opaque dropdown for mixer/Edge/stem use. Skin.ComboBox is the
// LateNight settings control (inner shadows, 4px inset, speech-bubble
// popup) and does not fit a 42px EQ column or a floating Edge toolbar.
ComboBox {
    id: root

    property int popupMaxHeight: 320
    property real popupWidth: Math.max(width, 150)
    property bool showIndicator: true

    font.pixelSize: 10

    background: Rectangle {
        border.color: (root.pressed || root.popup.visible) ? Theme.blue : (root.hovered ? Theme.deckTextColor : Theme.midGray)
        border.width: 1
        color: Theme.knobBackgroundColor
        radius: 3
    }
    contentItem: Text {
        clip: true
        color: Theme.deckTextColor
        elide: Text.ElideRight
        font: root.font
        leftPadding: 5
        rightPadding: root.showIndicator ? 14 : 4
        text: root.displayText
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Item {
        anchors.right: parent.right
        height: parent.height
        width: root.showIndicator ? 12 : 0

        Text {
            anchors.centerIn: parent
            color: Theme.deckTextColor
            font.pixelSize: 8
            text: "▾"
            visible: root.showIndicator
        }
    }
    delegate: ItemDelegate {
        id: itemDlgt

        required property int index

        highlighted: root.highlightedIndex === itemDlgt.index
        width: ListView.view ? ListView.view.width : root.popupWidth

        background: Rectangle {
            color: itemDlgt.highlighted ? Qt.rgba(0.004, 0.863, 0.988, 0.18) : "transparent"
        }
        contentItem: Text {
            color: Theme.deckTextColor
            elide: Text.ElideRight
            font.pixelSize: Math.max(root.font.pixelSize, 11)
            text: root.textAt(itemDlgt.index)
            verticalAlignment: Text.AlignVCenter
        }
    }
    popup: Popup {
        implicitHeight: Math.min(contentItem.implicitHeight, root.popupMaxHeight)
        padding: 4
        width: root.popupWidth
        y: root.height + 2

        background: Rectangle {
            border.color: Theme.midGray
            border.width: 1
            color: Theme.darkGray2
            radius: 4
        }
        contentItem: ListView {
            clip: true
            currentIndex: root.highlightedIndex
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null

            ScrollIndicator.vertical: ScrollIndicator {
            }
        }
    }
}
