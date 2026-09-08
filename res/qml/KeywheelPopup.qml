import "." as Skin
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme"

// Chrome around the Camelot wheel, in the New UI's window dress rather than
// the platform style: the main-window ground behind a 1px panel hairline with
// an 8px corner, and the 28px toolbar-grey header strip with an 11px bold
// uppercase label, a ✕ button and a blue underline. The reference is the
// header in Settings.qml; keeping the wheel itself free of chrome leaves it
// reusable on the Edge surface.
Popup {
    id: root

    // The wheel is square. The box is that square plus the padding, and taller
    // by the header strip and the gap under it.
    readonly property real chromeHeight: 28 + 8
    required property int numDecks

    // The popup had no way out but the menu item that opened it, so say so:
    // Escape, a click outside, or the button in the corner.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    focus: true
    height: Math.min(720, parent.height - 60, parent.width - 60 + root.chromeHeight)
    modal: false
    padding: 12
    width: height - root.chromeHeight
    // Centred by hand on `parent`, which -- because this is declared in
    // main.qml -- is the ApplicationWindow's content item. Two shortcuts that
    // look right and are not: `anchors.centerIn: parent` centres a Popup on
    // the parent's ORIGIN, not its middle (it landed at x=-342, y=-380), and
    // the `Window` attached property reads 0 on a Popup, collapsing the box.
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)

    background: Rectangle {
        border.color: Theme.panelBorderColor
        border.width: 1
        color: Theme.backgroundColor
        radius: 8
    }
    contentItem: ColumnLayout {
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            color: Theme.toolbarBackgroundColor
            implicitHeight: 28
            radius: 4

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.right: closeButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.deckTextColor
                elide: Text.ElideRight
                font.bold: true
                font.capitalization: Font.AllUppercase
                font.pixelSize: 11
                text: qsTr("Camelot Wheel")
            }
            Skin.Button {
                id: closeButton

                activeColor: Theme.white
                anchors.right: parent.right
                anchors.rightMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                fontPixelSize: 12
                implicitHeight: 22
                implicitWidth: 26
                text: "✕"

                onClicked: root.close()
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: Theme.blue
                height: 1
                opacity: 0.35
            }
        }
        Skin.Keywheel {
            Layout.fillHeight: true
            Layout.fillWidth: true
            numDecks: root.numDecks
        }
    }
}
