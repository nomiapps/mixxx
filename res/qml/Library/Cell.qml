import Qt5Compat.GraphicalEffects
import QtQuick
import QtQuick.Layouts
import "../Theme"

Rectangle {
    id: root

    // The item to grab for the drag image, built on first use.
    function prepareDragImage() {
        dragImageLoader.active = true;
        return dragImageLoader.item.effect;
    }

    Drag.dragType: Drag.Automatic
    Drag.mimeData: {
        "text/uri-list": file_url.toString(),
        "text/plain": file_url.toString()
    }
    Drag.supportedActions: Qt.CopyAction
    anchors.fill: parent
    color: selected
            ? Qt.rgba(0.004, 0.863, 0.988, 0.14)
            : (row % 2 == 0 ? Theme.sunkenBackgroundColor : Theme.backgroundColor)

    // The drag image -- a card with the cover, title and artist under a drop
    // shadow -- is only built when a drag starts. Every cell of every row had
    // one, shadow effect included, so building them all with the table was a
    // large part of what it cost to lay out a screenful of rows.
    Loader {
        id: dragImageLoader

        active: false

        sourceComponent: Item {
            readonly property alias effect: dragImageEffect

            Item {
                id: dragImageSource

                height: 85
                visible: false
                width: 190

                Rectangle {
                    color: Theme.sunkenBackgroundColor
                    radius: 12

                    anchors {
                        bottom: parent.bottom
                        left: parent.left
                        margins: 5
                        right: parent.right
                        top: parent.top
                    }
                    RowLayout {
                        anchors.fill: parent

                        Image {
                            id: cover

                            Layout.fillHeight: true
                            Layout.preferredWidth: cover_art ? 75 : 0
                            asynchronous: false
                            clip: true
                            fillMode: Image.PreserveAspectFit
                            // Decoded at twice the 75 px cell and no larger. Without a
                            // sourceSize the provider returns every cover at its full
                            // embedded resolution and Qt caches them all, so a screenful
                            // of rows costs whatever the artwork happens to be. Fixed
                            // rather than bound to the cell, so a resize does not reload
                            // them.
                            mipmap: true
                            source: cover_art
                            sourceSize.height: 150
                            sourceSize.width: 150
                        }
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true

                            Text {
                                color: Theme.textColor
                                text: track ? track.title : 'Unknown title'
                            }
                            Text {
                                color: Theme.midGray
                                text: track ? track.artist : 'Unknown artist'
                            }
                        }
                    }
                    Rectangle {
                        width: 20

                        gradient: Gradient {
                            orientation: Gradient.Horizontal

                            GradientStop {
                                color: Theme.darkGray
                                position: 1
                            }
                            GradientStop {
                                color: 'transparent'
                                position: 0
                            }
                        }

                        anchors {
                            bottom: parent.bottom
                            right: parent.right
                            top: parent.top
                        }
                    }
                }
            }
            DropShadow {
                id: dragImageEffect

                anchors.fill: dragImageSource
                color: "#80000000"
                horizontalOffset: 0
                radius: 10.0
                source: dragImageSource
                verticalOffset: 0
                visible: false
            }
        }
    }
    Rectangle {
        id: border

        color: Theme.darkGray2
        width: 1

        anchors {
            bottom: parent.bottom
            right: parent.right
            top: parent.top
        }
    }
}
